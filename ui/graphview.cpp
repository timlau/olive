#include "graphview.h"

#include <QPainter>
#include <QMouseEvent>

#include "panels/panels.h"
#include "panels/timeline.h"
#include "panels/viewer.h"
#include "project/sequence.h"
#include "project/effectrow.h"
#include "project/effectfield.h"
#include "ui/keyframedrawing.h"

#include "debug.h"

#define GRAPH_ZOOM_SPEED 0.05
#define GRAPH_SIZE 100

QColor get_curve_color(int index, int length) {
	QColor c;
	int hue = qRound((double(index)/double(length))*255);
	c.setHsv(hue, 255, 255);
	return c;
}

GraphView::GraphView(QWidget* parent) :
	QWidget(parent),
	x_scroll(0),
	y_scroll(0),
	mousedown(false),
	zoom(1.0),
	row(NULL)
{
	setMouseTracking(true);
}

void GraphView::paintEvent(QPaintEvent *event) {
	QPainter p(this);

	if (panel_sequence_viewer->seq != NULL) {
		// draw lines
		//int graph_size = GRAPH_SIZE*zoom;
		bool draw_text = true;//(fontMetrics().height() < graph_size && fontMetrics().width("0000") < graph_size);

		p.setPen(Qt::gray);
		int i = 0;
		while (true) {
			int line_x = (i*GRAPH_SIZE*zoom) - x_scroll;
			if (line_x >= width()) {
				break;
			}
			if (line_x >= 0) {
				if (line_x > 0) p.drawLine(line_x, 0, line_x, height());
				if (draw_text) p.drawText(QRect(line_x, height()-50, 50, 50), Qt::AlignBottom | Qt::AlignLeft, QString::number(i*GRAPH_SIZE));
			}
			i++;
		}
		i = 0;
		while (true) {
			int line_y = height() - (i*GRAPH_SIZE*zoom) + y_scroll;
			if (line_y <= 0) {
				break;
			}
			if (line_y <= height()) {
				if (line_y < height()) p.drawLine(0, line_y, width(), line_y);
				if (draw_text) p.drawText(QRect(0, line_y-50, 50, 50), Qt::AlignBottom | Qt::AlignLeft, QString::number(i*GRAPH_SIZE));
			}
			i++;
		}

		// draw keyframes
		if (row != NULL) {
			QPen line_pen;
			line_pen.setWidth(2);

			// sort keyframes by time
			QVector<int> sorted_keys;
			for (int i=0;i<row->keyframe_times.size();i++) {
				bool inserted = false;
				for (int j=0;j<sorted_keys.size();j++) {
					if (row->keyframe_times.at(sorted_keys.at(j)) > row->keyframe_times.at(i)) {
						sorted_keys.insert(j, i);
						inserted = true;
						break;
					}
				}
				if (!inserted) {
					sorted_keys.append(i);
				}
			}

			int last_key_x, last_key_y;
			for (int i=0;i<row->fieldCount();i++) {
				EffectField* field = row->field(i);
				if (field->type == EFFECT_FIELD_DOUBLE) {
					for (int j=0;j<sorted_keys.size();j++) {
						int key_index = sorted_keys.at(j);

						int key_x = get_screen_x(row->keyframe_times.at(key_index));
						int key_y = get_screen_y(field->keyframe_data.at(key_index).toDouble());

						line_pen.setColor(get_curve_color(i, row->fieldCount()));
						p.setPen(line_pen);
						if (key_index == 0) {
							p.drawLine(0, key_y, key_x, key_y);
						} else {
							p.drawLine(last_key_x, last_key_y, key_x, key_y);
						}
						last_key_x = key_x;
						last_key_y = key_y;
					}
					for (int j=0;j<sorted_keys.size();j++) {
						int key_index = sorted_keys.at(j);

						int key_x = get_screen_x(row->keyframe_times.at(key_index));
						int key_y = get_screen_y(field->keyframe_data.at(key_index).toDouble());

						draw_keyframe(p, row->keyframe_types.at(key_index), key_x, key_y, false);
					}
					p.setBrush(Qt::NoBrush);
				}
			}
		}

		// draw playhead
		p.setPen(Qt::red);
		int playhead_x = get_screen_x(panel_sequence_viewer->seq->playhead);
		p.drawLine(playhead_x, 0, playhead_x, height());
	}

	p.setPen(Qt::white);

	QRect border = rect();
	border.setWidth(border.width()-1);
	border.setHeight(border.height()-1);
	p.drawRect(border);
}

void GraphView::mousePressEvent(QMouseEvent *event) {
	mousedown = true;
	start_x = event->pos().x();
	start_y = event->pos().y();
}

void GraphView::mouseMoveEvent(QMouseEvent *event) {
	if (mousedown) {
		if (event->buttons() & Qt::MiddleButton || panel_timeline->tool == TIMELINE_TOOL_HAND) {
			set_scroll_x(x_scroll + start_x - event->pos().x());
			set_scroll_y(y_scroll + event->pos().y() - start_y);
			start_x = event->pos().x();
			start_y = event->pos().y();
			update();
		}
	}
}

void GraphView::mouseReleaseEvent(QMouseEvent *event) {
	mousedown = false;
}

void GraphView::wheelEvent(QWheelEvent *event) {
	bool redraw = false;
	bool shift = (event->modifiers() & Qt::ShiftModifier); // scroll instead of zoom
	bool alt = (event->modifiers() & Qt::AltModifier); // horiz scroll instead of vert scroll

	if (shift) {
		// scroll
		set_scroll_x(x_scroll + event->angleDelta().x()/10);
		set_scroll_y(y_scroll + event->angleDelta().y()/10);
		redraw = true;
	} else {
		// set zoom
		if (event->angleDelta().y() != 0) {
			double zoom_diff = (GRAPH_ZOOM_SPEED*zoom);
			if (event->angleDelta().y() < 0) zoom_diff = -zoom_diff;
			zoom += zoom_diff;
			emit zoom_changed(zoom);
			redraw = true;
		}
	}

	if (redraw) {
		update();
	}
}

void GraphView::set_row(EffectRow *r) {
	row = r;
	update();
}

void GraphView::set_scroll_x(int s) {
	x_scroll = s;//qMax(0, s);
	dout << x_scroll;
	emit x_scroll_changed(x_scroll);
}

void GraphView::set_scroll_y(int s) {
	y_scroll = s;//qMax(0, s);
	emit y_scroll_changed(y_scroll);
}

int GraphView::get_screen_x(double d) {
	return (d*zoom) - x_scroll;
}

int GraphView::get_screen_y(double d) {
	return height() + y_scroll - d*zoom;
}

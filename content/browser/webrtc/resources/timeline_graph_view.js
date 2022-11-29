// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maximum number of labels placed vertically along the sides of the graph.
const MAX_VERTICAL_LABELS = 6;

// Vertical spacing between labels and between the graph and labels.
const LABEL_VERTICAL_SPACING = 4;
// Horizontal spacing between vertically placed labels and the edges of the
// graph.
const LABEL_HORIZONTAL_SPACING = 3;
// Horizintal spacing between two horitonally placed labels along the bottom
// of the graph.
const LABEL_LABEL_HORIZONTAL_SPACING = 25;

// Length of ticks, in pixels, next to y-axis labels.  The x-axis only has
// one set of labels, so it can use lines instead.
const Y_AXIS_TICK_LENGTH = 10;

const GRID_COLOR = '#CCC';
const TEXT_COLOR = '#000';
const BACKGROUND_COLOR = '#FFF';

const MAX_DECIMAL_PRECISION = 3;

/**
 * A TimelineGraphView displays a timeline graph on a canvas element.
 */
export class TimelineGraphView {
  constructor(divId, canvasId) {
    this.scrollbar_ = {position_: 0, range_: 0};

    // Disable getElementById restriction here, since |divId| and |canvasId| are
    // not always valid selectors.
    // eslint-disable-next-line no-restricted-properties
    this.graphDiv_ = document.getElementById(divId);
    // eslint-disable-next-line no-restricted-properties
    this.canvas_ = document.getElementById(canvasId);

    // Set the range and scale of the graph.  Times are in milliseconds since
    // the Unix epoch.

    // All measurements we have must be after this time.
    this.startTime_ = 0;
    // The current rightmost position of the graph is always at most this.
    this.endTime_ = 1;

    this.graph_ = null;

    // Horizontal scale factor, in terms of milliseconds per pixel.
    this.scale_ = 1000;

    // Initialize the scrollbar.
    this.updateScrollbarRange_(true);
  }

  setScale(scale) {
    this.scale_ = scale;
  }

  // Returns the total length of the graph, in pixels.
  getLength_() {
    const timeRange = this.endTime_ - this.startTime_;
    // Math.floor is used to ignore the last partial area, of length less
    // than this.scale_.
    return Math.floor(timeRange / this.scale_);
  }

  /**
   * Returns true if the graph is scrolled all the way to the right.
   */
  graphScrolledToRightEdge_() {
    return this.scrollbar_.position_ === this.scrollbar_.range_;
  }

  /**
   * Update the range of the scrollbar.  If |resetPosition| is true, also
   * sets the slider to point at the rightmost position and triggers a
   * repaint.
   */
  updateScrollbarRange_(resetPosition) {
    let scrollbarRange = this.getLength_() - this.canvas_.width;
    if (scrollbarRange < 0) {
      scrollbarRange = 0;
    }

    // If we've decreased the range to less than the current scroll position,
    // we need to move the scroll position.
    if (this.scrollbar_.position_ > scrollbarRange) {
      resetPosition = true;
    }

    this.scrollbar_.range_ = scrollbarRange;
    if (resetPosition) {
      this.scrollbar_.position_ = scrollbarRange;
      this.repaint();
    }
  }

  /**
   * Sets the date range displayed on the graph, switches to the default
   * scale factor, and moves the scrollbar all the way to the right.
   */
  setDateRange(startDate, endDate) {
    this.startTime_ = startDate.getTime();
    this.endTime_ = endDate.getTime();

    // Safety check.
    if (this.endTime_ <= this.startTime_) {
      this.startTime_ = this.endTime_ - 1;
    }

    this.updateScrollbarRange_(true);
  }

  /**
   * Updates the end time at the right of the graph to be the current time.
   * Specifically, updates the scrollbar's range, and if the scrollbar is
   * all the way to the right, keeps it all the way to the right.  Otherwise,
   * leaves the view as-is and doesn't redraw anything.
   */
  updateEndDate(opt_date) {
    this.endTime_ = opt_date || (new Date()).getTime();
    this.updateScrollbarRange_(this.graphScrolledToRightEdge_());
  }

  getStartDate() {
    return new Date(this.startTime_);
  }

  /**
   * Replaces the current TimelineDataSeries with |dataSeries|.
   */
  setDataSeries(dataSeries) {
    // Simply recreates the Graph.
    this.graph_ = new Graph();
    for (let i = 0; i < dataSeries.length; ++i) {
      this.graph_.addDataSeries(dataSeries[i]);
    }
    this.repaint();
  }

  /**
   * Adds |dataSeries| to the current graph.
   */
  addDataSeries(dataSeries) {
    if (!this.graph_) {
      this.graph_ = new Graph();
    }
    this.graph_.addDataSeries(dataSeries);
    this.repaint();
  }

  /**
   * Draws the graph on |canvas_| when visible.
   */
  repaint() {
    if (this.canvas_.offsetParent === null) {
      return;  // do not repaint graphs that are not visible.
    }

    this.repaintTimerRunning_ = false;

    const width = this.canvas_.width;
    let height = this.canvas_.height;
    const context = this.canvas_.getContext('2d');

    // Clear the canvas.
    context.fillStyle = BACKGROUND_COLOR;
    context.fillRect(0, 0, width, height);

    // Try to get font height in pixels.  Needed for layout.
    const fontHeightString = context.font.match(/([0-9]+)px/)[1];
    const fontHeight = parseInt(fontHeightString);

    // Safety check, to avoid drawing anything too ugly.
    if (fontHeightString.length === 0 || fontHeight <= 0 ||
        fontHeight * 4 > height || width < 50) {
      return;
    }

    // Save current transformation matrix so we can restore it later.
    context.save();

    // The center of an HTML canvas pixel is technically at (0.5, 0.5).  This
    // makes near straight lines look bad, due to anti-aliasing.  This
    // translation reduces the problem a little.
    context.translate(0.5, 0.5);

    // Figure out what time values to display.
    let position = this.scrollbar_.position_;
    // If the entire time range is being displayed, align the right edge of
    // the graph to the end of the time range.
    if (this.scrollbar_.range_ === 0) {
      position = this.getLength_() - this.canvas_.width;
    }
    const visibleStartTime = this.startTime_ + position * this.scale_;

    // Make space at the bottom of the graph for the time labels, and then
    // draw the labels.
    const textHeight = height;
    height -= fontHeight + LABEL_VERTICAL_SPACING;
    this.drawTimeLabels(context, width, height, textHeight, visibleStartTime);

    // Draw outline of the main graph area.
    context.strokeStyle = GRID_COLOR;
    context.strokeRect(0, 0, width - 1, height - 1);

    if (this.graph_) {
      // Layout graph and have them draw their tick marks.
      this.graph_.layout(
          width, height, fontHeight, visibleStartTime, this.scale_);
      this.graph_.drawTicks(context);

      // Draw the lines of all graphs, and then draw their labels.
      this.graph_.drawLines(context);
      this.graph_.drawLabels(context);
    }

    // Restore original transformation matrix.
    context.restore();
  }

  /**
   * Draw time labels below the graph.  Takes in start time as an argument
   * since it may not be |startTime_|, when we're displaying the entire
   * time range.
   */
  drawTimeLabels(context, width, height, textHeight, startTime) {
    // Draw the labels 1 minute apart.
    const timeStep = 1000 * 60;

    // Find the time for the first label.  This time is a perfect multiple of
    // timeStep because of how UTC times work.
    let time = Math.ceil(startTime / timeStep) * timeStep;

    context.textBaseline = 'bottom';
    context.textAlign = 'center';
    context.fillStyle = TEXT_COLOR;
    context.strokeStyle = GRID_COLOR;

    // Draw labels and vertical grid lines.
    while (true) {
      const x = Math.round((time - startTime) / this.scale_);
      if (x >= width) {
        break;
      }
      const text = (new Date(time)).toLocaleTimeString();
      context.fillText(text, x, textHeight);
      context.beginPath();
      context.lineTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
      time += timeStep;
    }
  }

  getDataSeriesCount() {
    if (this.graph_) {
      return this.graph_.dataSeries_.length;
    }
    return 0;
  }

  hasDataSeries(dataSeries) {
    if (this.graph_) {
      return this.graph_.hasDataSeries(dataSeries);
    }
    return false;
  }
}

/**
 * A Label is the label at a particular position along the y-axis.
 */
class Label {
  constructor(height, text) {
    this.height = height;
    this.text = text;
  }
}

/**
 * A Graph is responsible for drawing all the TimelineDataSeries that have
 * the same data type.  Graphs are responsible for scaling the values, laying
 * out labels, and drawing both labels and lines for its data series.
 */
class Graph {
  constructor() {
    this.dataSeries_ = [];

    // Cached properties of the graph, set in layout.
    this.width_ = 0;
    this.height_ = 0;
    this.fontHeight_ = 0;
    this.startTime_ = 0;
    this.scale_ = 0;

    // The lowest/highest values adjusted by the vertical label step size
    // in the displayed range of the graph. Used for scaling and setting
    // labels.  Set in layoutLabels.
    this.min_ = 0;
    this.max_ = 0;

    // Cached text of equally spaced labels.  Set in layoutLabels.
    this.labels_ = [];
  }

  addDataSeries(dataSeries) {
    this.dataSeries_.push(dataSeries);
  }

  hasDataSeries(dataSeries) {
    for (let i = 0; i < this.dataSeries_.length; ++i) {
      if (this.dataSeries_[i] === dataSeries) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns a list of all the values that should be displayed for a given
   * data series, using the current graph layout.
   */
  getValues(dataSeries) {
    if (!dataSeries.isVisible()) {
      return null;
    }
    return dataSeries.getValues(this.startTime_, this.scale_, this.width_);
  }

  /**
   * Updates the graph's layout.  In particular, both the max value and
   * label positions are updated.  Must be called before calling any of the
   * drawing functions.
   */
  layout(width, height, fontHeight, startTime, scale) {
    this.width_ = width;
    this.height_ = height;
    this.fontHeight_ = fontHeight;
    this.startTime_ = startTime;
    this.scale_ = scale;

    // Find largest value.
    let max = 0;
    let min = 0;
    for (let i = 0; i < this.dataSeries_.length; ++i) {
      const values = this.getValues(this.dataSeries_[i]);
      if (!values) {
        continue;
      }
      for (let j = 0; j < values.length; ++j) {
        if (values[j] > max) {
          max = values[j];
        } else if (values[j] < min) {
          min = values[j];
        }
      }
    }

    this.layoutLabels_(min, max);
  }

  /**
   * Lays out labels and sets |max_|/|min_|, taking the time units into
   * consideration.  |maxValue| is the actual maximum value, and
   * |max_| will be set to the value of the largest label, which
   * will be at least |maxValue|. Similar for |min_|.
   */
  layoutLabels_(minValue, maxValue) {
    if (maxValue - minValue < 1024) {
      this.layoutLabelsBasic_(minValue, maxValue, MAX_DECIMAL_PRECISION);
      return;
    }

    // Find appropriate units to use.
    const units = ['', 'k', 'M', 'G', 'T', 'P'];
    // Units to use for labels.  0 is '1', 1 is K, etc.
    // We start with 1, and work our way up.
    let unit = 1;
    minValue /= 1024;
    maxValue /= 1024;
    while (units[unit + 1] && maxValue - minValue >= 1024) {
      minValue /= 1024;
      maxValue /= 1024;
      ++unit;
    }

    // Calculate labels.
    this.layoutLabelsBasic_(minValue, maxValue, MAX_DECIMAL_PRECISION);

    // Append units to labels.
    for (let i = 0; i < this.labels_.length; ++i) {
      this.labels_[i] += ' ' + units[unit];
    }

    // Convert |min_|/|max_| back to unit '1'.
    this.min_ *= Math.pow(1024, unit);
    this.max_ *= Math.pow(1024, unit);
  }

  /**
   * Same as layoutLabels_, but ignores units.  |maxDecimalDigits| is the
   * maximum number of decimal digits allowed.  The minimum allowed
   * difference between two adjacent labels is 10^-|maxDecimalDigits|.
   */
  layoutLabelsBasic_(minValue, maxValue, maxDecimalDigits) {
    this.labels_ = [];
    const range = maxValue - minValue;
    // No labels if the range is 0.
    if (range === 0) {
      this.min_ = this.max_ = maxValue;
      return;
    }

    // The maximum number of equally spaced labels allowed.  |fontHeight_|
    // is doubled because the top two labels are both drawn in the same
    // gap.
    const minLabelSpacing = 2 * this.fontHeight_ + LABEL_VERTICAL_SPACING;

    // The + 1 is for the top label.
    let maxLabels = 1 + this.height_ / minLabelSpacing;
    if (maxLabels < 2) {
      maxLabels = 2;
    } else if (maxLabels > MAX_VERTICAL_LABELS) {
      maxLabels = MAX_VERTICAL_LABELS;
    }

    // Initial try for step size between consecutive labels.
    let stepSize = Math.pow(10, -maxDecimalDigits);
    // Number of digits to the right of the decimal of |stepSize|.
    // Used for formatting label strings.
    let stepSizeDecimalDigits = maxDecimalDigits;

    // Pick a reasonable step size.
    while (true) {
      // If we use a step size of |stepSize| between labels, we'll need:
      //
      // Math.ceil(range / stepSize) + 1
      //
      // labels.  The + 1 is because we need labels at both at 0 and at
      // the top of the graph.

      // Check if we can use steps of size |stepSize|.
      if (Math.ceil(range / stepSize) + 1 <= maxLabels) {
        break;
      }
      // Check |stepSize| * 2.
      if (Math.ceil(range / (stepSize * 2)) + 1 <= maxLabels) {
        stepSize *= 2;
        break;
      }
      // Check |stepSize| * 5.
      if (Math.ceil(range / (stepSize * 5)) + 1 <= maxLabels) {
        stepSize *= 5;
        break;
      }
      stepSize *= 10;
      if (stepSizeDecimalDigits > 0) {
        --stepSizeDecimalDigits;
      }
    }

    // Set the min/max so it's an exact multiple of the chosen step size.
    this.max_ = Math.ceil(maxValue / stepSize) * stepSize;
    this.min_ = Math.floor(minValue / stepSize) * stepSize;

    // Create labels.
    for (let label = this.max_; label >= this.min_; label -= stepSize) {
      this.labels_.push(label.toFixed(stepSizeDecimalDigits));
    }
  }

  /**
   * Draws tick marks for each of the labels in |labels_|.
   */
  drawTicks(context) {
    const x1 = this.width_ - 1;
    const x2 = this.width_ - 1 - Y_AXIS_TICK_LENGTH;

    context.fillStyle = GRID_COLOR;
    context.beginPath();
    for (let i = 1; i < this.labels_.length - 1; ++i) {
      // The rounding is needed to avoid ugly 2-pixel wide anti-aliased
      // lines.
      const y = Math.round(this.height_ * i / (this.labels_.length - 1));
      context.moveTo(x1, y);
      context.lineTo(x2, y);
    }
    context.stroke();
  }

  /**
   * Draws a graph line for each of the data series.
   */
  drawLines(context) {
    // Factor by which to scale all values to convert them to a number from
    // 0 to height - 1.
    let scale = 0;
    const bottom = this.height_ - 1;
    if (this.max_) {
      scale = bottom / (this.max_ - this.min_);
    }

    // Draw in reverse order, so earlier data series are drawn on top of
    // subsequent ones.
    for (let i = this.dataSeries_.length - 1; i >= 0; --i) {
      const values = this.getValues(this.dataSeries_[i]);
      if (!values) {
        continue;
      }
      context.strokeStyle = this.dataSeries_[i].getColor();
      context.beginPath();
      for (let x = 0; x < values.length; ++x) {
        // The rounding is needed to avoid ugly 2-pixel wide anti-aliased
        // horizontal lines.
        context.lineTo(x, bottom - Math.round((values[x] - this.min_) * scale));
      }
      context.stroke();
    }
  }

  /**
   * Draw labels in |labels_|.
   */
  drawLabels(context) {
    if (this.labels_.length === 0) {
      return;
    }
    const x = this.width_ - LABEL_HORIZONTAL_SPACING;

    // Set up the context.
    context.fillStyle = TEXT_COLOR;
    context.textAlign = 'right';

    // Draw top label, which is the only one that appears below its tick
    // mark.
    context.textBaseline = 'top';
    context.fillText(this.labels_[0], x, 0);

    // Draw all the other labels.
    context.textBaseline = 'bottom';
    const step = (this.height_ - 1) / (this.labels_.length - 1);
    for (let i = 1; i < this.labels_.length; ++i) {
      context.fillText(this.labels_[i], x, step * i);
    }
  }
}

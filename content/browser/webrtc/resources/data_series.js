// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The maximum number of data points buffered for each stats. Old data points
// will be shifted out when the buffer is full.
export const MAX_STATS_DATA_POINT_BUFFER_SIZE = 1000;

/**
 * A TimelineDataSeries collects an ordered series of (time, value) pairs,
 * and converts them to graph points.  It also keeps track of its color and
 * current visibility state.
 * It keeps MAX_STATS_DATA_POINT_BUFFER_SIZE data points at most. Old data
 * points will be dropped when it reaches this size.
 */
export class TimelineDataSeries {
  constructor(statsType) {
    // List of DataPoints in chronological order.
    this.dataPoints_ = [];

    // Default color.  Should always be overridden prior to display.
    this.color_ = 'red';
    // Whether or not the data series should be drawn.
    this.isVisible_ = true;

    this.cacheStartTime_ = null;
    this.cacheStepSize_ = 0;
    this.cacheValues_ = [];
    this.statsType_ = statsType;
  }

  /**
   * @override
   */
  toJSON() {
    if (this.dataPoints_.length < 1) {
      return {};
    }

    const values = [];
    for (let i = 0; i < this.dataPoints_.length; ++i) {
      values.push(this.dataPoints_[i].value);
    }
    return {
      startTime: this.dataPoints_[0].time,
      endTime: this.dataPoints_[this.dataPoints_.length - 1].time,
      statsType: this.statsType_,
      values: JSON.stringify(values),
    };
  }

  /**
   * Adds a DataPoint to |this| with the specified time and value.
   * DataPoints are assumed to be received in chronological order.
   */
  addPoint(timeTicks, value) {
    const time = new Date(timeTicks);
    this.dataPoints_.push(new DataPoint(time, value));

    if (this.dataPoints_.length > MAX_STATS_DATA_POINT_BUFFER_SIZE) {
      this.dataPoints_.shift();
    }
  }

  isVisible() {
    return this.isVisible_;
  }

  show(isVisible) {
    this.isVisible_ = isVisible;
  }

  getColor() {
    return this.color_;
  }

  setColor(color) {
    this.color_ = color;
  }

  getCount() {
    return this.dataPoints_.length;
  }
  /**
   * Returns a list containing the values of the data series at |count|
   * points, starting at |startTime|, and |stepSize| milliseconds apart.
   * Caches values, so showing/hiding individual data series is fast.
   */
  getValues(startTime, stepSize, count) {
    // Use cached values, if we can.
    if (this.cacheStartTime_ === startTime &&
        this.cacheStepSize_ === stepSize &&
        this.cacheValues_.length === count) {
      return this.cacheValues_;
    }

    // Do all the work.
    this.cacheValues_ = this.getValuesInternal_(startTime, stepSize, count);
    this.cacheStartTime_ = startTime;
    this.cacheStepSize_ = stepSize;

    return this.cacheValues_;
  }

  /**
   * Returns the cached |values| in the specified time period.
   */
  getValuesInternal_(startTime, stepSize, count) {
    const values = [];
    let nextPoint = 0;
    let currentValue = 0;
    let time = startTime;
    for (let i = 0; i < count; ++i) {
      while (nextPoint < this.dataPoints_.length &&
             this.dataPoints_[nextPoint].time < time) {
        currentValue = this.dataPoints_[nextPoint].value;
        ++nextPoint;
      }
      values[i] = currentValue;
      time += stepSize;
    }
    return values;
  }
}

/**
 * A single point in a data series.  Each point has a time, in the form of
 * milliseconds since the Unix epoch, and a numeric value.
 */
class DataPoint {
  constructor(time, value) {
    this.time = time;
    this.value = value;
  }
}

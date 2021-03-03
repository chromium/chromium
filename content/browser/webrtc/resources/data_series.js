// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A TimelineDataSeries collects an ordered series of (time, value) pairs,
 * and converts them to graph points.  It also keeps track of its color and
 * current visibility state.
 * It keeps MAX_STATS_DATA_POINT_BUFFER_SIZE data points at most. Old data
 * points will be dropped when it reaches this size.
 */
var TimelineDataSeries = (function() {
  'use strict';

  /**
   * @constructor
   */
  function TimelineDataSeries() {
    // List of DataPoints in chronological order.
    this.dataPoints_ = [];

    // Default color.  Should always be overridden prior to display.
    this.color_ = 'red';
    // Whether or not the data series should be drawn.
    this.isVisible_ = true;

    this.cacheStartTime_ = null;
    this.cacheStepSize_ = 0;
    this.cacheValues_ = [];
  }

  TimelineDataSeries.prototype = {
    /**
     * @override
     */
    toJSON: function() {
      if (this.dataPoints_.length < 1) {
        return {};
      }

      var values = [];
      for (var i = 0; i < this.dataPoints_.length; ++i) {
        values.push(this.dataPoints_[i].value);
      }
      return {
        startTime: this.dataPoints_[0].time,
        endTime: this.dataPoints_[this.dataPoints_.length - 1].time,
        values: JSON.stringify(values),
      };
    },

    /**
     * Adds a DataPoint to |this| with the specified time and value.
     * DataPoints are assumed to be received in chronological order.
     */
    addPoint: function(timeTicks, value) {
      var time = new Date(timeTicks);
      this.dataPoints_.push(new DataPoint(time, value));

      if (this.dataPoints_.length > MAX_STATS_DATA_POINT_BUFFER_SIZE) {
        this.dataPoints_.shift();
      }
    },

    isVisible: function() {
      return this.isVisible_;
    },

    show: function(isVisible) {
      this.isVisible_ = isVisible;
    },

    getColor: function() {
      return this.color_;
    },

    setColor: function(color) {
      this.color_ = color;
    },

    getCount: function() {
      return this.dataPoints_.length;
    },
    /**
     * Returns a list containing the values of the data series at |count|
     * points, starting at |startTime|, and |stepSize| milliseconds apart.
     * Caches values, so showing/hiding individual data series is fast.
     */
    getValues: function(startTime, stepSize, count) {
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
    },

    /**
     * Returns the cached |values| in the specified time period.
     */
    getValuesInternal_: function(startTime, stepSize, count) {
      var values = [];
      var nextPoint = 0;
      var currentValue = 0;
      var time = startTime;
      for (var i = 0; i < count; ++i) {
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
  };

  /**
   * A single point in a data series.  Each point has a time, in the form of
   * milliseconds since the Unix epoch, and a numeric value.
   * @constructor
   */
  function DataPoint(time, value) {
    this.time = time;
    this.value = value;
  }

  return TimelineDataSeries;
})();

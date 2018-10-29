// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @unrestricted
 */
Profiler.HeapTimelineOverview = class extends UI.VBox {
  constructor() {
    super();
    this.element.id = 'heap-recording-view';
    this.element.classList.add('heap-tracking-overview');

    this._overviewContainer = this.element.createChild('div', 'heap-overview-container');
    this._overviewGrid = new PerfUI.OverviewGrid('heap-recording');
    this._overviewGrid.element.classList.add('fill');

    this._overviewCanvas = this._overviewContainer.createChild('canvas', 'heap-recording-overview-canvas');
    this._overviewContainer.appendChild(this._overviewGrid.element);
    this._overviewCalculator = new Profiler.HeapTimelineOverview.OverviewCalculator();
    this._overviewGrid.addEventListener(PerfUI.OverviewGrid.Events.WindowChanged, this._onWindowChanged, this);

    this._windowLeft = 0.0;
    this._windowRight = 1.0;
    this._overviewGrid.setWindow(this._windowLeft, this._windowRight);
    this._yScale = new Profiler.HeapTimelineOverview.SmoothScale();
    this._xScale = new Profiler.HeapTimelineOverview.SmoothScale();

    this._profileSamples = new Profiler.HeapTimelineOverview.Samples();
  }

  /**
   * @param {!Profiler.HeapTimelineOverview.Samples} samples
   */
  setSamples(samples) {
    this._profileSamples = samples;
    this.update();
  }

  /**
   * @param {number} width
   * @param {number} height
   */
  _drawOverviewCanvas(width, height) {
    if (!this._profileSamples)
      return;
    const profileSamples = this._profileSamples;
    const sizes = profileSamples.sizes;
    const topSizes = profileSamples.max;
    const timestamps = profileSamples.timestamps;
    const startTime = timestamps[0];
    const endTime = timestamps[timestamps.length - 1];

    const scaleFactor = this._xScale.nextScale(width / profileSamples.totalTime);
    let maxSize = 0;
    /**
     * @param {!Array.<number>} sizes
     * @param {function(number, number):void} callback
     */
    function aggregateAndCall(sizes, callback) {
      let size = 0;
      let currentX = 0;
      for (let i = 1; i < timestamps.length; ++i) {
        const x = Math.floor((timestamps[i] - startTime) * scaleFactor);
        if (x !== currentX) {
          if (size)
            callback(currentX, size);
          size = 0;
          currentX = x;
        }
        size += sizes[i];
      }
      callback(currentX, size);
    }

    /**
     * @param {number} x
     * @param {number} size
     */
    function maxSizeCallback(x, size) {
      maxSize = Math.max(maxSize, size);
    }

    aggregateAndCall(sizes, maxSizeCallback);

    const yScaleFactor = this._yScale.nextScale(maxSize ? height / (maxSize * 1.1) : 0.0);

    this._overviewCanvas.width = width * window.devicePixelRatio;
    this._overviewCanvas.height = height * window.devicePixelRatio;
    this._overviewCanvas.style.width = width + 'px';
    this._overviewCanvas.style.height = height + 'px';

    const context = this._overviewCanvas.getContext('2d');
    context.scale(window.devicePixelRatio, window.devicePixelRatio);

    context.beginPath();
    context.lineWidth = 2;
    context.strokeStyle = 'rgba(192, 192, 192, 0.6)';
    const currentX = (endTime - startTime) * scaleFactor;
    context.moveTo(currentX, height - 1);
    context.lineTo(currentX, 0);
    context.stroke();
    context.closePath();

    let gridY;
    let gridValue;
    const gridLabelHeight = 14;
    if (yScaleFactor) {
      const maxGridValue = (height - gridLabelHeight) / yScaleFactor;
      // The round value calculation is a bit tricky, because
      // it has a form k*10^n*1024^m, where k=[1,5], n=[0..3], m is an integer,
      // e.g. a round value 10KB is 10240 bytes.
      gridValue = Math.pow(1024, Math.floor(Math.log(maxGridValue) / Math.log(1024)));
      gridValue *= Math.pow(10, Math.floor(Math.log(maxGridValue / gridValue) / Math.LN10));
      if (gridValue * 5 <= maxGridValue)
        gridValue *= 5;
      gridY = Math.round(height - gridValue * yScaleFactor - 0.5) + 0.5;
      context.beginPath();
      context.lineWidth = 1;
      context.strokeStyle = 'rgba(0, 0, 0, 0.2)';
      context.moveTo(0, gridY);
      context.lineTo(width, gridY);
      context.stroke();
      context.closePath();
    }

    /**
     * @param {number} x
     * @param {number} size
     */
    function drawBarCallback(x, size) {
      context.moveTo(x, height - 1);
      context.lineTo(x, Math.round(height - size * yScaleFactor - 1));
    }

    context.beginPath();
    context.lineWidth = 2;
    context.strokeStyle = 'rgba(192, 192, 192, 0.6)';
    aggregateAndCall(topSizes, drawBarCallback);
    context.stroke();
    context.closePath();

    context.beginPath();
    context.lineWidth = 2;
    context.strokeStyle = 'rgba(0, 0, 192, 0.8)';
    aggregateAndCall(sizes, drawBarCallback);
    context.stroke();
    context.closePath();

    if (gridValue) {
      const label = Number.bytesToString(gridValue);
      const labelPadding = 4;
      const labelX = 0;
      const labelY = gridY - 0.5;
      const labelWidth = 2 * labelPadding + context.measureText(label).width;
      context.beginPath();
      context.textBaseline = 'bottom';
      context.font = '10px ' + window.getComputedStyle(this.element, null).getPropertyValue('font-family');
      context.fillStyle = 'rgba(255, 255, 255, 0.75)';
      context.fillRect(labelX, labelY - gridLabelHeight, labelWidth, gridLabelHeight);
      context.fillStyle = 'rgb(64, 64, 64)';
      context.fillText(label, labelX + labelPadding, labelY);
      context.fill();
      context.closePath();
    }
  }

  /**
   * @override
   */
  onResize() {
    this._updateOverviewCanvas = true;
    this._scheduleUpdate();
  }

  _onWindowChanged() {
    if (!this._updateGridTimerId)
      this._updateGridTimerId = setTimeout(this.updateGrid.bind(this), 10);
  }

  _scheduleUpdate() {
    if (this._updateTimerId)
      return;
    this._updateTimerId = setTimeout(this.update.bind(this), 10);
  }

  _updateBoundaries() {
    this._windowLeft = this._overviewGrid.windowLeft();
    this._windowRight = this._overviewGrid.windowRight();
    this._windowWidth = this._windowRight - this._windowLeft;
  }

  update() {
    this._updateTimerId = null;
    if (!this.isShowing())
      return;
    this._updateBoundaries();
    this._overviewCalculator._updateBoundaries(this);
    this._overviewGrid.updateDividers(this._overviewCalculator);
    this._drawOverviewCanvas(this._overviewContainer.clientWidth, this._overviewContainer.clientHeight - 20);
  }

  updateGrid() {
    this._updateGridTimerId = 0;
    this._updateBoundaries();
    const ids = this._profileSamples.ids;
    if (!ids.length)
      return;
    const timestamps = this._profileSamples.timestamps;
    const sizes = this._profileSamples.sizes;
    const startTime = timestamps[0];
    const totalTime = this._profileSamples.totalTime;
    const timeLeft = startTime + totalTime * this._windowLeft;
    const timeRight = startTime + totalTime * this._windowRight;
    const minIndex = timestamps.lowerBound(timeLeft);
    const maxIndex = timestamps.upperBound(timeRight);
    let size = 0;
    for (let i = minIndex; i < maxIndex; ++i)
      size += sizes[i];
    const minId = minIndex < ids.length ? ids[minIndex] : Infinity;
    const maxId = maxIndex < ids.length ? ids[maxIndex] : Infinity;

    this.dispatchEventToListeners(Profiler.HeapTimelineOverview.IdsRangeChanged, {minId, maxId, size});
  }
};

Profiler.HeapTimelineOverview.IdsRangeChanged = Symbol('IdsRangeChanged');

Profiler.HeapTimelineOverview.SmoothScale = class {
  constructor() {
    this._lastUpdate = 0;
    this._currentScale = 0.0;
  }

  /**
   * @param {number} target
   * @return {number}
   */
  nextScale(target) {
    target = target || this._currentScale;
    if (this._currentScale) {
      const now = Date.now();
      const timeDeltaMs = now - this._lastUpdate;
      this._lastUpdate = now;
      const maxChangePerSec = 20;
      const maxChangePerDelta = Math.pow(maxChangePerSec, timeDeltaMs / 1000);
      const scaleChange = target / this._currentScale;
      this._currentScale *= Number.constrain(scaleChange, 1 / maxChangePerDelta, maxChangePerDelta);
    } else {
      this._currentScale = target;
    }
    return this._currentScale;
  }
};

/**
 * @unrestricted
 */
Profiler.HeapTimelineOverview.Samples = class {
  constructor() {
    /** @type {!Array<number>} */
    this.sizes = [];
    /** @type {!Array<number>} */
    this.ids = [];
    /** @type {!Array<number>} */
    this.timestamps = [];
    /** @type {!Array<number>} */
    this.max = [];
    /** @type {number} */
    this.totalTime = 30000;
  }
};

/**
 * @implements {PerfUI.TimelineGrid.Calculator}
 * @unrestricted
 */
Profiler.HeapTimelineOverview.OverviewCalculator = class {
  /**
   * @param {!Profiler.HeapTimelineOverview} chart
   */
  _updateBoundaries(chart) {
    this._minimumBoundaries = 0;
    this._maximumBoundaries = chart._profileSamples.totalTime;
    this._xScaleFactor = chart._overviewContainer.clientWidth / this._maximumBoundaries;
  }

  /**
   * @override
   * @param {number} time
   * @return {number}
   */
  computePosition(time) {
    return (time - this._minimumBoundaries) * this._xScaleFactor;
  }

  /**
   * @override
   * @param {number} value
   * @param {number=} precision
   * @return {string}
   */
  formatValue(value, precision) {
    return Number.secondsToString(value / 1000, !!precision);
  }

  /**
   * @override
   * @return {number}
   */
  maximumBoundary() {
    return this._maximumBoundaries;
  }

  /**
   * @override
   * @return {number}
   */
  minimumBoundary() {
    return this._minimumBoundaries;
  }

  /**
   * @override
   * @return {number}
   */
  zeroTime() {
    return this._minimumBoundaries;
  }

  /**
   * @override
   * @return {number}
   */
  boundarySpan() {
    return this._maximumBoundaries - this._minimumBoundaries;
  }
};

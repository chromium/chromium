// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {SDK.SDKModelObserver}
 * @unrestricted
 */
PerformanceMonitor.PerformanceMonitor = class extends UI.HBox {
  constructor() {
    super(true);
    this.registerRequiredCSS('performance_monitor/performanceMonitor.css');
    this.contentElement.classList.add('perfmon-pane');
    /** @type {!Array<!{timestamp: number, metrics: !Map<string, number>}>} */
    this._metricsBuffer = [];
    /** @const */
    this._pixelsPerMs = 10 / 1000;
    /** @const */
    this._pollIntervalMs = 500;
    /** @const */
    this._scaleHeight = 16;
    /** @const */
    this._graphHeight = 90;
    this._gridColor = UI.themeSupport.patchColorText('rgba(0, 0, 0, 0.08)', UI.ThemeSupport.ColorUsage.Foreground);
    this._controlPane = new PerformanceMonitor.PerformanceMonitor.ControlPane(this.contentElement);
    const chartContainer = this.contentElement.createChild('div', 'perfmon-chart-container');
    this._canvas = /** @type {!HTMLCanvasElement} */ (chartContainer.createChild('canvas'));
    this.contentElement.createChild('div', 'perfmon-chart-suspend-overlay fill').createChild('div').textContent =
        Common.UIString('Paused');
    this._controlPane.addEventListener(
        PerformanceMonitor.PerformanceMonitor.ControlPane.Events.MetricChanged, this._recalcChartHeight, this);
    SDK.targetManager.observeModels(SDK.PerformanceMetricsModel, this);
  }

  /**
   * @override
   */
  wasShown() {
    if (!this._model)
      return;
    SDK.targetManager.addEventListener(SDK.TargetManager.Events.SuspendStateChanged, this._suspendStateChanged, this);
    this._model.enable();
    this._suspendStateChanged();
  }

  /**
   * @override
   */
  willHide() {
    if (!this._model)
      return;
    SDK.targetManager.removeEventListener(
        SDK.TargetManager.Events.SuspendStateChanged, this._suspendStateChanged, this);
    this._stopPolling();
    this._model.disable();
  }

  /**
   * @override
   * @param {!SDK.PerformanceMetricsModel} model
   */
  modelAdded(model) {
    if (this._model)
      return;
    this._model = model;
    if (this.isShowing())
      this.wasShown();
  }

  /**
   * @override
   * @param {!SDK.PerformanceMetricsModel} model
   */
  modelRemoved(model) {
    if (this._model !== model)
      return;
    if (this.isShowing())
      this.willHide();
    this._model = null;
  }

  _suspendStateChanged() {
    const suspended = SDK.targetManager.allTargetsSuspended();
    if (suspended)
      this._stopPolling();
    else
      this._startPolling();
    this.contentElement.classList.toggle('suspended', suspended);
  }

  _startPolling() {
    this._startTimestamp = 0;
    this._pollTimer = setInterval(() => this._poll(), this._pollIntervalMs);
    this.onResize();
    animate.call(this);

    /**
     * @this {PerformanceMonitor.PerformanceMonitor}
     */
    function animate() {
      this._draw();
      this._animationId = this.contentElement.window().requestAnimationFrame(animate.bind(this));
    }
  }

  _stopPolling() {
    clearInterval(this._pollTimer);
    this.contentElement.window().cancelAnimationFrame(this._animationId);
    this._metricsBuffer = [];
  }

  async _poll() {
    const data = await this._model.requestMetrics();
    const timestamp = data.timestamp;
    const metrics = data.metrics;
    this._metricsBuffer.push({timestamp, metrics: metrics});
    const millisPerWidth = this._width / this._pixelsPerMs;
    // Multiply by 2 as the pollInterval has some jitter and to have some extra samples if window is resized.
    const maxCount = Math.ceil(millisPerWidth / this._pollIntervalMs * 2);
    if (this._metricsBuffer.length > maxCount * 2)  // Multiply by 2 to have a hysteresis.
      this._metricsBuffer.splice(0, this._metricsBuffer.length - maxCount);
    this._controlPane.updateMetrics(metrics);
  }

  _draw() {
    const ctx = /** @type {!CanvasRenderingContext2D} */ (this._canvas.getContext('2d'));
    ctx.save();
    ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    ctx.clearRect(0, 0, this._width, this._height);
    ctx.save();
    ctx.translate(0, this._scaleHeight);  // Reserve space for the scale bar.
    for (const chartInfo of this._controlPane.charts()) {
      if (!this._controlPane.isActive(chartInfo.metrics[0].name))
        continue;
      this._drawChart(ctx, chartInfo, this._graphHeight);
      ctx.translate(0, this._graphHeight);
    }
    ctx.restore();
    this._drawHorizontalGrid(ctx);
    ctx.restore();
  }

  /**
   * @param {!CanvasRenderingContext2D} ctx
   */
  _drawHorizontalGrid(ctx) {
    const labelDistanceSeconds = 10;
    const lightGray = UI.themeSupport.patchColorText('rgba(0, 0, 0, 0.02)', UI.ThemeSupport.ColorUsage.Foreground);
    ctx.font = '10px ' + Host.fontFamily();
    ctx.fillStyle = UI.themeSupport.patchColorText('rgba(0, 0, 0, 0.3)', UI.ThemeSupport.ColorUsage.Foreground);
    const currentTime = Date.now() / 1000;
    for (let sec = Math.ceil(currentTime);; --sec) {
      const x = this._width - ((currentTime - sec) * 1000 - this._pollIntervalMs) * this._pixelsPerMs;
      if (x < -50)
        break;
      ctx.beginPath();
      ctx.moveTo(Math.round(x) + 0.5, 0);
      ctx.lineTo(Math.round(x) + 0.5, this._height);
      if (sec >= 0 && sec % labelDistanceSeconds === 0)
        ctx.fillText(new Date(sec * 1000).toLocaleTimeString(), Math.round(x) + 4, 12);
      ctx.strokeStyle = sec % labelDistanceSeconds ? lightGray : this._gridColor;
      ctx.stroke();
    }
  }

  /**
   * @param {!CanvasRenderingContext2D} ctx
   * @param {!PerformanceMonitor.PerformanceMonitor.ChartInfo} chartInfo
   * @param {number} height
   */
  _drawChart(ctx, chartInfo, height) {
    ctx.save();
    ctx.rect(0, 0, this._width, height);
    ctx.clip();
    const bottomPadding = 8;
    const extraSpace = 1.05;
    const max = this._calcMax(chartInfo) * extraSpace;
    const stackedChartBaseLandscape = chartInfo.stacked ? new Map() : null;
    const paths = [];
    for (let i = chartInfo.metrics.length - 1; i >= 0; --i) {
      const metricInfo = chartInfo.metrics[i];
      paths.push({
        path: this._buildMetricPath(
            chartInfo, metricInfo, height - bottomPadding, max, i ? stackedChartBaseLandscape : null),
        color: metricInfo.color
      });
    }
    const backgroundColor =
        Common.Color.parse(UI.themeSupport.patchColorText('white', UI.ThemeSupport.ColorUsage.Background));
    for (const path of paths.reverse()) {
      const color = path.color;
      ctx.save();
      ctx.fillStyle = backgroundColor.blendWith(Common.Color.parse(color).setAlpha(0.2)).asString(null);
      ctx.fill(path.path);
      ctx.strokeStyle = color;
      ctx.lineWidth = 0.5;
      ctx.stroke(path.path);
      ctx.restore();
    }
    this._drawVerticalGrid(ctx, height - bottomPadding, max, chartInfo);
    ctx.restore();
  }

  /**
   * @param {!PerformanceMonitor.PerformanceMonitor.ChartInfo} chartInfo
   * @return {number}
   */
  _calcMax(chartInfo) {
    if (chartInfo.max)
      return chartInfo.max;
    const width = this._width;
    const startTime = performance.now() - this._pollIntervalMs - width / this._pixelsPerMs;
    let max = -Infinity;
    for (const metricInfo of chartInfo.metrics) {
      for (let i = this._metricsBuffer.length - 1; i >= 0; --i) {
        const metrics = this._metricsBuffer[i];
        const value = metrics.metrics.get(metricInfo.name);
        max = Math.max(max, value);
        if (metrics.timestamp < startTime)
          break;
      }
    }
    if (!this._metricsBuffer.length)
      return 10;

    const base10 = Math.pow(10, Math.floor(Math.log10(max)));
    max = Math.ceil(max / base10 / 2) * base10 * 2;

    const alpha = 0.2;
    chartInfo.currentMax = max * alpha + (chartInfo.currentMax || max) * (1 - alpha);
    return chartInfo.currentMax;
  }

  /**
   * @param {!CanvasRenderingContext2D} ctx
   * @param {number} height
   * @param {number} max
   * @param {!PerformanceMonitor.PerformanceMonitor.ChartInfo} info
   */
  _drawVerticalGrid(ctx, height, max, info) {
    let base = Math.pow(10, Math.floor(Math.log10(max)));
    const firstDigit = Math.floor(max / base);
    if (firstDigit !== 1 && firstDigit % 2 === 1)
      base *= 2;
    let scaleValue = Math.floor(max / base) * base;

    const span = max;
    const topPadding = 5;
    const visibleHeight = height - topPadding;
    ctx.fillStyle = UI.themeSupport.patchColorText('rgba(0, 0, 0, 0.3)', UI.ThemeSupport.ColorUsage.Foreground);
    ctx.strokeStyle = this._gridColor;
    ctx.beginPath();
    for (let i = 0; i < 2; ++i) {
      const y = calcY(scaleValue);
      const labelText = PerformanceMonitor.PerformanceMonitor.MetricIndicator._formatNumber(scaleValue, info);
      ctx.moveTo(0, y);
      ctx.lineTo(4, y);
      ctx.moveTo(ctx.measureText(labelText).width + 12, y);
      ctx.lineTo(this._width, y);
      ctx.fillText(labelText, 8, calcY(scaleValue) + 3);
      scaleValue /= 2;
    }
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(0, height + 0.5);
    ctx.lineTo(this._width, height + 0.5);
    ctx.strokeStyle = UI.themeSupport.patchColorText('rgba(0, 0, 0, 0.2)', UI.ThemeSupport.ColorUsage.Foreground);
    ctx.stroke();
    /**
     * @param {number} value
     * @return {number}
     */
    function calcY(value) {
      return Math.round(height - visibleHeight * value / span) + 0.5;
    }
  }

  /**
   * @param {!PerformanceMonitor.PerformanceMonitor.ChartInfo} chartInfo
   * @param {!PerformanceMonitor.PerformanceMonitor.MetricInfo} metricInfo
   * @param {number} height
   * @param {number} scaleMax
   * @param {?Map<number, number>} stackedChartBaseLandscape
   * @return {!Path2D}
   */
  _buildMetricPath(chartInfo, metricInfo, height, scaleMax, stackedChartBaseLandscape) {
    const path = new Path2D();
    const topPadding = 5;
    const visibleHeight = height - topPadding;
    if (visibleHeight < 1)
      return path;
    const span = scaleMax;
    const metricName = metricInfo.name;
    const pixelsPerMs = this._pixelsPerMs;
    const startTime = performance.now() - this._pollIntervalMs - this._width / pixelsPerMs;
    const smooth = chartInfo.smooth;

    let x = 0;
    let lastY = 0;
    let lastX = 0;
    if (this._metricsBuffer.length) {
      x = (this._metricsBuffer[0].timestamp - startTime) * pixelsPerMs;
      path.moveTo(x, calcY(0));
      path.lineTo(this._width + 5, calcY(0));
      lastY = calcY(this._metricsBuffer.peekLast().metrics.get(metricName));
      lastX = this._width + 5;
      path.lineTo(lastX, lastY);
    }
    for (let i = this._metricsBuffer.length - 1; i >= 0; --i) {
      const metrics = this._metricsBuffer[i];
      const timestamp = metrics.timestamp;
      let value = metrics.metrics.get(metricName);
      if (stackedChartBaseLandscape) {
        value += stackedChartBaseLandscape.get(timestamp) || 0;
        value = Number.constrain(value, 0, 1);
        stackedChartBaseLandscape.set(timestamp, value);
      }
      const y = calcY(value);
      x = (timestamp - startTime) * pixelsPerMs;
      if (smooth) {
        const midX = (lastX + x) / 2;
        path.bezierCurveTo(midX, lastY, midX, y, x, y);
      } else {
        path.lineTo(x, lastY);
        path.lineTo(x, y);
      }
      lastX = x;
      lastY = y;
      if (timestamp < startTime)
        break;
    }
    return path;

    /**
     * @param {number} value
     * @return {number}
     */
    function calcY(value) {
      return Math.round(height - visibleHeight * value / span) + 0.5;
    }
  }

  /**
   * @override
   */
  onResize() {
    super.onResize();
    this._width = this._canvas.offsetWidth;
    this._canvas.width = Math.round(this._width * window.devicePixelRatio);
    this._recalcChartHeight();
  }

  _recalcChartHeight() {
    let height = this._scaleHeight;
    for (const chartInfo of this._controlPane.charts()) {
      if (this._controlPane.isActive(chartInfo.metrics[0].name))
        height += this._graphHeight;
    }
    this._height = Math.ceil(height * window.devicePixelRatio);
    this._canvas.height = this._height;
    this._canvas.style.height = `${this._height / window.devicePixelRatio}px`;
  }
};

/** @enum {symbol} */
PerformanceMonitor.PerformanceMonitor.Format = {
  Percent: Symbol('Percent'),
  Bytes: Symbol('Bytes'),
};

/**
 * @typedef {!{
 *   title: string,
 *   metrics: !Array<!PerformanceMonitor.PerformanceMonitor.MetricInfo>,
 *   max: (number|undefined),
 *   currentMax: (number|undefined),
 *   format: (!PerformanceMonitor.PerformanceMonitor.Format|undefined),
 *   smooth: (boolean|undefined)
 * }}
 */
PerformanceMonitor.PerformanceMonitor.ChartInfo;

/**
 * @typedef {!{
 *   name: string,
 *   color: string
 * }}
 */
PerformanceMonitor.PerformanceMonitor.MetricInfo;

PerformanceMonitor.PerformanceMonitor.ControlPane = class extends Common.Object {
  /**
   * @param {!Element} parent
   */
  constructor(parent) {
    super();
    this.element = parent.createChild('div', 'perfmon-control-pane');

    this._enabledChartsSetting =
        Common.settings.createSetting('perfmonActiveIndicators2', ['TaskDuration', 'JSHeapTotalSize', 'Nodes']);
    /** @type {!Set<string>} */
    this._enabledCharts = new Set(this._enabledChartsSetting.get());
    const format = PerformanceMonitor.PerformanceMonitor.Format;

    /** @type {!Array<!PerformanceMonitor.PerformanceMonitor.ChartInfo>} */
    this._chartsInfo = [
      {
        title: Common.UIString('CPU usage'),
        metrics: [
          {name: 'TaskDuration', color: '#999'}, {name: 'ScriptDuration', color: 'orange'},
          {name: 'LayoutDuration', color: 'blueviolet'}, {name: 'RecalcStyleDuration', color: 'violet'}
        ],
        format: format.Percent,
        smooth: true,
        stacked: true,
        color: 'red',
        max: 1
      },
      {
        title: Common.UIString('JS heap size'),
        metrics: [{name: 'JSHeapTotalSize', color: '#99f'}, {name: 'JSHeapUsedSize', color: 'blue'}],
        format: format.Bytes,
        color: 'blue'
      },
      {title: Common.UIString('DOM Nodes'), metrics: [{name: 'Nodes', color: 'green'}]},
      {title: Common.UIString('JS event listeners'), metrics: [{name: 'JSEventListeners', color: 'yellowgreen'}]},
      {title: Common.UIString('Documents'), metrics: [{name: 'Documents', color: 'darkblue'}]},
      {title: Common.UIString('Document Frames'), metrics: [{name: 'Frames', color: 'darkcyan'}]},
      {title: Common.UIString('Layouts / sec'), metrics: [{name: 'LayoutCount', color: 'hotpink'}]},
      {title: Common.UIString('Style recalcs / sec'), metrics: [{name: 'RecalcStyleCount', color: 'deeppink'}]}
    ];
    for (const info of this._chartsInfo) {
      for (const metric of info.metrics)
        metric.color = UI.themeSupport.patchColorText(metric.color, UI.ThemeSupport.ColorUsage.Foreground);
    }

    /** @type {!Map<string, !PerformanceMonitor.PerformanceMonitor.MetricIndicator>} */
    this._indicators = new Map();
    for (const chartInfo of this._chartsInfo) {
      const chartName = chartInfo.metrics[0].name;
      const active = this._enabledCharts.has(chartName);
      const indicator = new PerformanceMonitor.PerformanceMonitor.MetricIndicator(
          this.element, chartInfo, active, this._onToggle.bind(this, chartName));
      this._indicators.set(chartName, indicator);
    }
  }

  /**
   * @param {string} chartName
   * @param {boolean} active
   */
  _onToggle(chartName, active) {
    if (active)
      this._enabledCharts.add(chartName);
    else
      this._enabledCharts.delete(chartName);
    this._enabledChartsSetting.set(Array.from(this._enabledCharts));
    this.dispatchEventToListeners(PerformanceMonitor.PerformanceMonitor.ControlPane.Events.MetricChanged);
  }

  /**
   * @return {!Array<!PerformanceMonitor.PerformanceMonitor.ChartInfo>}
   */
  charts() {
    return this._chartsInfo;
  }

  /**
   * @param {string} metricName
   * @return {boolean}
   */
  isActive(metricName) {
    return this._enabledCharts.has(metricName);
  }

  /**
   * @param {!Map<string, number>} metrics
   */
  updateMetrics(metrics) {
    for (const name of this._indicators.keys()) {
      if (metrics.has(name))
        this._indicators.get(name).setValue(metrics.get(name));
    }
  }
};

/** @enum {symbol} */
PerformanceMonitor.PerformanceMonitor.ControlPane.Events = {
  MetricChanged: Symbol('MetricChanged')
};

PerformanceMonitor.PerformanceMonitor.MetricIndicator = class {
  /**
   * @param {!Element} parent
   * @param {!PerformanceMonitor.PerformanceMonitor.ChartInfo} info
   * @param {boolean} active
   * @param {function(boolean)} onToggle
   */
  constructor(parent, info, active, onToggle) {
    const color = info.color || info.metrics[0].color;
    this._info = info;
    this._active = active;
    this._onToggle = onToggle;
    this.element = parent.createChild('div', 'perfmon-indicator');
    this._swatchElement = UI.Icon.create('smallicon-checkmark-square', 'perfmon-indicator-swatch');
    this._swatchElement.style.backgroundColor = color;
    this.element.appendChild(this._swatchElement);
    this.element.createChild('div', 'perfmon-indicator-title').textContent = info.title;
    this._valueElement = this.element.createChild('div', 'perfmon-indicator-value');
    this._valueElement.style.color = color;
    this.element.addEventListener('click', () => this._toggleIndicator());
    this.element.classList.toggle('active', active);
  }

  /**
   * @param {number} value
   * @param {!PerformanceMonitor.PerformanceMonitor.ChartInfo} info
   * @return {string}
   */
  static _formatNumber(value, info) {
    if (!PerformanceMonitor.PerformanceMonitor.MetricIndicator._numberFormatter) {
      PerformanceMonitor.PerformanceMonitor.MetricIndicator._numberFormatter =
          new Intl.NumberFormat('en-US', {maximumFractionDigits: 1});
      PerformanceMonitor.PerformanceMonitor.MetricIndicator._percentFormatter =
          new Intl.NumberFormat('en-US', {maximumFractionDigits: 1, style: 'percent'});
    }
    switch (info.format) {
      case PerformanceMonitor.PerformanceMonitor.Format.Percent:
        return PerformanceMonitor.PerformanceMonitor.MetricIndicator._percentFormatter.format(value);
      case PerformanceMonitor.PerformanceMonitor.Format.Bytes:
        return Number.bytesToString(value);
      default:
        return PerformanceMonitor.PerformanceMonitor.MetricIndicator._numberFormatter.format(value);
    }
  }

  /**
   * @param {number} value
   */
  setValue(value) {
    this._valueElement.textContent =
        PerformanceMonitor.PerformanceMonitor.MetricIndicator._formatNumber(value, this._info);
  }

  _toggleIndicator() {
    this._active = !this._active;
    this.element.classList.toggle('active', this._active);
    this._onToggle(this._active);
  }
};

PerformanceMonitor.PerformanceMonitor.MetricIndicator._format =
    new Intl.NumberFormat('en-US', {maximumFractionDigits: 1});

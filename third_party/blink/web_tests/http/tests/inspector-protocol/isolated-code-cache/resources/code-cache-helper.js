// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared helper for the isolated-code-cache tests. It records traces and
// prints the V8 compile / code cache trace events emitted by Blink while
// loading scripts (see third_party/blink/renderer/bindings/core/v8/).
(class CodeCacheHelper {
  constructor(testRunner, session, eventNames) {
    this._testRunner = testRunner;
    this._session = session;
    this._eventNames = new Set(eventNames);
  }

  async startTracing() {
    await this._session.protocol.Tracing.start(
        {categories: '-*,devtools.timeline', type: '', options: ''});
  }

  async stopTracingAndPrint() {
    const dp = this._session.protocol;
    const events = [];
    const collect = ({params}) => events.push(...params.value);
    dp.Tracing.onDataCollected(collect);
    dp.Tracing.end();
    await dp.Tracing.onceTracingComplete();
    dp.Tracing.offDataCollected(collect);
    events.filter(event => this._eventNames.has(event.name))
        .sort((a, b) => a.ts - b.ts)
        .forEach(event => this._printEvent(event));
  }

  // Prints the trace events recorded so far, logs `msg` and starts recording
  // the trace events for the next step.
  async expectationComment(msg) {
    await this.stopTracingAndPrint();
    this._testRunner.log(msg);
    await this.startTracing();
  }

  // Adds an iframe to the page and waits for its load event, which waits for
  // the <script> loading.
  addIframe(src, id = '') {
    function addIframeInPage(src, id) {
      return new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.src = src;
        if (id) {
          iframe.id = id;
        }
        iframe.onload = () => resolve();
        document.body.appendChild(iframe);
      });
    }
    return this._session.evaluateAsync(addIframeInPage,
                                       this._testRunner.url(src), id);
  }

  waitUntilIdle() {
    return this._session.evaluateAsync(
        'new Promise(resolve => requestIdleCallback(resolve))');
  }

  _printEvent(event) {
    const {scriptId, ...data} = event.args.data;
    const result = {
      data,
      endTime: event.ts + (event.dur || 0),
      startTime: event.ts,
      type: event.name,
    };
    const unstableNames = [
      'streamed',
      'consumedCacheSize',
      'producedCacheSize',
      'startTime',
      'endTime',
    ];
    this._testRunner.log(`${event.name} Properties:`);
    this._testRunner.log(result, '', unstableNames);
  }
})

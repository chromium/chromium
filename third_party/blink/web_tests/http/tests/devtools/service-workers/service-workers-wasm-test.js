// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as TimelineModel from 'devtools/models/timeline_model/timeline_model.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests V8 code cache for WebAssembly resources using Service Workers.\n`);

  await ApplicationTestRunner.resetState();
  SDK.NetworkManager.MultitargetNetworkManager.instance().clearBrowserCache();

  await TestRunner.showPanel('resources');
  await TestRunner.showPanel('timeline');

  await TestRunner.evaluateInPagePromise(`
      function registerServiceWorkerAndwaitForActivated() {
        const script = 'resources/wasm-cache-worker.js';
        const scope = 'resources/wasm-cache-iframe.html';
        return registerServiceWorker(script, scope)
          .then(() => waitForActivated(scope));
      }
      function loadModules() {
        const frameId = 'frame_id';
        let iframeWindow = document.getElementById(frameId).contentWindow;
        return iframeWindow.loadModules()
          .then(() => iframeWindow.loadModules());
      }
  `);

  const scope = 'resources/wasm-cache-iframe.html';

  await PerformanceTestRunner.invokeAsyncWithTimeline('registerServiceWorkerAndwaitForActivated');
  await ApplicationTestRunner.waitForActivated(scope);

  async function runTests() {
    // Asynchronous function to initiate tests in an iframe, and wait until
    // compilation has finished.
    const loadFrame = (url) => new Promise((resolve) => {
      function receiveMessage(e) {
        if (e.data == 'done') {
          resolve(e);
          window.removeEventListener('message', receiveMessage);
        }
      }
      window.addEventListener('message', receiveMessage);
      const iframe = document.createElement('iframe');
      iframe.src = url;
      document.body.appendChild(iframe);
    });

    // Test same-origin.
    await loadFrame('resources/wasm-cache-iframe.html');

    let script = document.createElement('script');
    script.type = 'module';
    script.text = 'window.finishTest()';
    document.body.appendChild(script);
    return new Promise(resolve => window.finishTest = resolve);
  }

  await TestRunner.evaluateInPagePromise(runTests.toString());

  await PerformanceTestRunner.invokeWithTracing('runTests', processEvents);

  function stringCompare(a, b) {
    return a > b ? 1 : b > a ? -1 : 0;
  }
  function processEvents() {
    // Since some WebAssembly compile events may be reported on different
    // threads, sort events by URL and type, to get a deterministic test.
    function compareEvents(a, b) {
      let url_a = a.args['url'] || '';
      let url_b = b.args['url'] || '';
      if (url_a != url_b)
        return stringCompare(url_a, url_b)

      return stringCompare(a.name, b.name)
    }

    const event_types = new Set([
      TimelineModel.TimelineModel.RecordType.WasmStreamFromResponseCallback,
      TimelineModel.TimelineModel.RecordType.WasmCompiledModule,
      TimelineModel.TimelineModel.RecordType.WasmCachedModule,
      TimelineModel.TimelineModel.RecordType.WasmModuleCacheHit,
      TimelineModel.TimelineModel.RecordType.WasmModuleCacheInvalid]);
    const tracingModel = PerformanceTestRunner.tracingModel();

    let events = new Array();
    PerformanceTestRunner.tracingModel().sortedProcesses().forEach(
      p => p.sortedThreads().forEach(
        t => events = events.concat(t.events().filter(event => event_types.has(event.name)))));
    events.sort(compareEvents);
    events.forEach(PerformanceTestRunner.printTraceEventProperties);

    TestRunner.completeTest();
  }
})();

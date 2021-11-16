// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests V8 code cache for WebAssembly resources.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  // Clear browser cache to avoid any existing entries for the fetched
  // scripts in the cache.
  SDK.multitargetNetworkManager.clearBrowserCache();

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
    await loadFrame('http://127.0.0.1:8000/wasm/resources/wasm-cache-iframe.html');
    // Ensure another origin must recompile everything.
    await loadFrame('http://localhost:8000/wasm/resources/wasm-cache-iframe.html');

    let script = document.createElement('script');
    script.type = 'module';
    script.text = 'window.finishTest()';
    document.body.appendChild(script);
    return new Promise(resolve => window.finishTest = resolve);
  }

  await TestRunner.evaluateInPagePromise(runTests.toString());

  TestRunner.addResult(

`WebAssembly trace events may be generated on multiple background threads, so
the test sorts them by URL and type to make the output deterministic. We fetch
2 small and 2 large .wasm resources, from 2 different origins. From these
8 fetches, we expect:

v8.wasm.cachedModule: 2 .wasm modules are cached
v8.wasm.streamFromResponseCallback: 8 .wasm resources are fetched
v8.wasm.compiledModule: 2 for large.wasm, 4 for small.wasm
v8.wasm.moduleCacheHit: 2 for large.wasm
`
);

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

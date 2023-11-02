// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the instrumentation of a UserTiming events\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
    function makeUserTimings()
    {
      let fulfill;
      const promise = new Promise(resolve => fulfill = resolve);

      setTimeout(_ => performance.mark('astart'), 0);
      setTimeout(_ => performance.mark('aend'), 100);

      setTimeout(_ => performance.mark('bstart'), 20);
      setTimeout(_ => performance.mark('bend'),  220);

      setTimeout(_ => performance.mark('cstart'), 40);
      setTimeout(_ => performance.mark('cend'),   41);

      setTimeout(_ => {
        performance.measure('timespan', 'astart', 'aend');
        performance.measure('timespan', 'bstart', 'bend');
        performance.measure('timespan', 'cstart', 'cend');
      }, 250);

      setTimeout(fulfill, 300);
      return promise;
    }
  `);

  PerformanceTestRunner.invokeWithTracing('makeUserTimings', TestRunner.safeWrap(onTracingComplete));
  TestRunner.addResult(`Expecting three measures with durations (in this order): 100, 200, 0`);

  var userTimingEventCount = 0;
  function onTracingComplete() {

    PerformanceTestRunner.tracingModel().sortedProcesses().forEach(process => {
      process.sortedThreads().forEach(thread => thread.asyncEvents().forEach(processAsyncEvent));
    });

    TestRunner.assertEquals(3, userTimingEventCount);
    TestRunner.completeTest();

    function processAsyncEvent(event) {
      if (!event.parsedCategories.has('blink.user_timing')) return;

      const roundedDuration = Math.round(event.duration / 100) * 100;
      TestRunner.addResult(`Got Async Event. Duration: ${roundedDuration}`);
      ++userTimingEventCount;
    }
  }
})();

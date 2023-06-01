// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests that worker events are recorded with proper devtools metadata events.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      // Save references to the worker objects to make sure they are not GC'ed.
      var worker1;
      var worker2;

      function startFirstWorker()
      {
          worker1 = new Worker("resources/worker.js");
          worker1.postMessage("");
          return new Promise((fulfill) => worker1.onmessage = fulfill);
      }

      function startSecondWorker()
      {
          worker2 = new Worker("resources/worker.js");
          worker2.postMessage("");
          return new Promise((fulfill) => worker2.onmessage = fulfill);
      }
  `);

  await TestRunner.evaluateInPageAsync(`startFirstWorker()`);

  PerformanceTestRunner.invokeWithTracing('startSecondWorker', TestRunner.safeWrap(processTracingEvents));

  var workerMetadataEventCount = 0;
  function processTracingEvents() {
    PerformanceTestRunner.tracingModel().sortedProcesses().forEach(function(process) {
      process.sortedThreads().forEach(function(thread) {
        thread.events().forEach(processEvent);
      });
    });
    TestRunner.assertEquals(2, workerMetadataEventCount);
    TestRunner.completeTest();
  }

  function processEvent(event) {
    if (!event.hasCategory(SDK.TracingModel.DevToolsMetadataEventCategory) ||
        event.name !== TimelineModel.TimelineModel.DevToolsMetadataEvent.TracingSessionIdForWorker)
      return;

    ++workerMetadataEventCount;
    TestRunner.addResult('Got DevTools worker metadata event(' + workerMetadataEventCount + '): ' + event.name);
  }
})();

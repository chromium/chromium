// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests that worker events are properly filtered in timeline.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      // Save references to the worker objects to make sure they are not GC'ed.
      let worker1;
      let worker2;

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
  await PerformanceTestRunner.invokeAsyncWithTimeline('startSecondWorker');

  // TODO(1034374): We flakily get 1 worker event instead of 2. Why do we even
  // expect 2 when the timeline is only recording during startSecondWorker()?
  const allEvents = PerformanceTestRunner.timelineModel().inspectedTargetEvents();
  const workerEvents = allEvents.filter(e => e.name === TimelineModel.TimelineModel.DevToolsMetadataEvent.TracingSessionIdForWorker);
  TestRunner.addResult(`Got ${workerEvents.length} worker metadata events`);
  TestRunner.completeTest();
})();

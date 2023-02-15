// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for scheduler.postTask()\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          function task() {
            controller.abort();
          }

          const controller = new TaskController({priority: 'background'});
          const signal = controller.signal;

          const p1 = scheduler.postTask(task);
          const p2 = scheduler.postTask(() => {}, {signal});
          p2.catch(() => {});

          return p1;
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  await PerformanceTestRunner.printTimelineRecordsWithDetails('SchedulePostTaskCallback');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('RunPostTaskCallback');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('AbortPostTaskCallback');
  TestRunner.completeTest();
})();

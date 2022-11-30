// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for Timers\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var timerOne = setTimeout("1 + 1", 10);
          var timerTwo = setInterval(intervalTimerWork, 20);
          var iteration = 0;

          function intervalTimerWork()
          {
              if (++iteration < 2)
                  return;
              clearInterval(timerTwo);
              callback();
          }
          return promise;
      }
  `);

  UI.panels.timeline.disableCaptureJSProfileSetting.set(true);
  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  await PerformanceTestRunner.printTimelineRecordsWithDetails('TimerInstall');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('TimerFire');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('TimerRemove');
  PerformanceTestRunner.printTimelineRecords('FunctionCall');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('EvaluateScript');
  TestRunner.completeTest();
})();

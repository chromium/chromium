// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for Animation Frame feature\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var requestId = window.requestAnimationFrame(animationFrameCallback);
          function animationFrameCallback()
          {
              window.cancelAnimationFrame(requestId);
              if (callback)
                  callback();
          }
          return promise;
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  await PerformanceTestRunner.printTimelineRecordsWithDetails('RequestAnimationFrame');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('FireAnimationFrame');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('CancelAnimationFrame');
  TestRunner.completeTest();
})();

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a script tag with an external script.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var script = document.createElement("script");
          script.src = "resources/timeline-script-tag-2.js";
          document.body.appendChild(script);
      }
  `);

  await PerformanceTestRunner.startTimeline();
  TestRunner.evaluateInPage('performActions()');
  await ConsoleTestRunner.waitUntilMessageReceivedPromise();

  await PerformanceTestRunner.stopTimeline();
  PerformanceTestRunner.printTimelineRecords('EvaluateScript');
  TestRunner.completeTest();
})();

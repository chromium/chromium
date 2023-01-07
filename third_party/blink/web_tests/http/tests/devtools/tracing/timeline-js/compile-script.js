// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline instrumentation for CompileScript event.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions() {
        return new Promise(resolve => {
          var script = document.createElement("script");
          script.textContent = "function noop1() {} \\n//# sourceURL=script-content.js";
          document.body.appendChild(script);
          eval("function noop2() {} \\n//# sourceURL=script-content.js");

          script = document.createElement("script");
          script.src = "resources/timeline-script-tag-2.js";
          script.onload = resolve;
          document.body.appendChild(script);
        });
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');
  await PerformanceTestRunner.printTimelineRecordsWithDetails(TimelineModel.TimelineModel.RecordType.CompileScript);
  TestRunner.completeTest();
})();

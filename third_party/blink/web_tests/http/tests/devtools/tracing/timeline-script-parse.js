// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for v8.parseOnBackground\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var script = document.createElement("script");
          script.src = "resources/timeline-script-parse.php";
          script.async = true;
          script.onload = callback;
          document.body.appendChild(script);
          return promise;
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  const tracingModel = PerformanceTestRunner.tracingModel();
  tracingModel.sortedProcesses().forEach(p => p.sortedThreads().forEach(t => t.events().forEach(event => {
    if (event.name === TimelineModel.TimelineModel.RecordType.ParseScriptOnBackground)
      PerformanceTestRunner.printTraceEventPropertiesWithDetails(event);
  })));
  TestRunner.completeTest();
})();

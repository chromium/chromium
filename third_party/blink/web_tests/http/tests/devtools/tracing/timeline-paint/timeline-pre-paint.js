// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the instrumentation of PrePaint event\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <style>
      .layer {
          position: absolute;
          width: 20px;
          height: 20px;
          background: green;
          will-change: transform;
      }
      </style>
      <div id="parent-layer"></div>
    `);
  await TestRunner.addScriptTag('../../../resources/run-after-layout-and-paint.js');
  await TestRunner.evaluateInPagePromise(`
      function doActions()
      {
          var layer = document.createElement("div");
          layer.classList.add("layer");
          document.getElementById("parent-layer").appendChild(layer);
          return new Promise((fulfill) => runAfterLayoutAndPaint(fulfill));
      }
  `);

  PerformanceTestRunner.invokeWithTracing('doActions', onTracingComplete);
  function onTracingComplete() {
    var events = PerformanceTestRunner.timelineModel().inspectedTargetEvents();
    for (var i = 0; i < events.length; ++i) {
      var event = events[i];
      if (events[i].name === TimelineModel.TimelineModel.RecordType.PrePaint) {
        TestRunner.addResult('Got PrePaint event, phase: ' + events[i].phase);
        break;
      }
    }
    TestRunner.addResult('Done');
    TestRunner.completeTest();
  }
})();

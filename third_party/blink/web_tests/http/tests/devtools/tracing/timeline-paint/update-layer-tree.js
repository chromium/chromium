// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the instrumentation of UpdateLayerTree event\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <style>
      .layer {
          position: absolute;
          width: 20px;
          height: 20px;
          transform: translateZ(10px);
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
      if (events[i].name === TimelineModel.TimelineModel.RecordType.UpdateLayerTree) {
        TestRunner.addResult('Got UpdateLayerTree event, phase: ' + events[i].phase);
        break;
      }
    }
    TestRunner.addResult('Done');
    TestRunner.completeTest();
  }
})();

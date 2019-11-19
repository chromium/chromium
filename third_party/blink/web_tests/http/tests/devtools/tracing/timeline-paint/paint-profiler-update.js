// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that paint profiler is properly update when an event is selected in Flame Chart\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
    <div id="square" style="width: 40px; height: 40px"></div>
  `);
  await TestRunner.addScriptTag('../../../resources/run-after-layout-and-paint.js');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var square = document.getElementById("square");
          step1();

          function step1()
          {
              square.style.backgroundColor = "red";
              runAfterLayoutAndPaint(step2);
          }

          function step2()
          {
              square.style.backgroundColor = "black";
              runAfterLayoutAndPaint(callback);
          }
          return promise;
      }
  `);

  const panel = UI.panels.timeline;
  panel._captureLayersAndPicturesSetting.set(true);
  panel._onModeChanged();

  var paintEvents = [];
  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');
  var events = PerformanceTestRunner.mainTrackEvents();
  for (var event of events) {
    if (event.name === TimelineModel.TimelineModel.RecordType.Paint) {
      paintEvents.push(event);
      if (!TimelineModel.TimelineData.forEvent(event).picture)
        TestRunner.addResult('Event without picture at ' + paintEvents.length);
    }
  }

  if (paintEvents.length < 2)
    throw new Error('FAIL: Expect at least two paint events');

  TestRunner.addSniffer(
      panel._flameChart._detailsView, '_appendDetailsTabsForTraceEventAndShowDetails', onRecordDetailsReady, false);
  panel.select(Timeline.TimelineSelection.fromTraceEvent(paintEvents[0]));

  function onRecordDetailsReady() {
    var updateCount = 0;

    panel._flameChart._detailsView._tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.PaintProfiler, true);
    var paintProfilerView = panel._flameChart._detailsView._lazyPaintProfilerView._paintProfilerView;
    TestRunner.addSniffer(paintProfilerView, '_update', onPaintProfilerUpdate, true);

    function onPaintProfilerUpdate() {
      // No snapshot, not a real update yet -- wait for another update!
      if (!paintProfilerView._snapshot)
        return;
      var logSize = paintProfilerView._log && paintProfilerView._log.length ? '>0' : '0';
      TestRunner.addResult('Paint ' + updateCount + ' log size: ' + logSize);
      if (updateCount++)
        TestRunner.completeTest();
      else
        panel.select(
            Timeline.TimelineSelection.fromTraceEvent(paintEvents[1]), Timeline.TimelineDetailsView.Tab.PaintProfiler);
    }
  }
})();

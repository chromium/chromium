// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that paint profiler is properly update when an event is selected in Flame Chart\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
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
  panel.captureLayersAndPicturesSetting.set(true);
  panel.onModeChanged();

  var paintEvents = [];
  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');
  var events = PerformanceTestRunner.mainTrackEvents();
  for (var event of events) {
    // When CompositeAfterPaint is enabled, a Paint trace event will be
    // generated which encompasses the entire paint cycle for the page. That
    // event will not correspond to any captured picture, and we just ignore it
    // for the purpose of this test.
    if (event.name === TimelineModel.TimelineModel.RecordType.Paint &&
        TimelineModel.TimelineData.forEvent(event).picture) {
      paintEvents.push(event);
    }
  }

  if (paintEvents.length < 2)
    throw new Error('FAIL: Expect at least two paint events');

  TestRunner.addSniffer(
      panel.flameChart.detailsView, 'appendDetailsTabsForTraceEventAndShowDetails', onRecordDetailsReady, false);
  panel.select(Timeline.TimelineSelection.fromTraceEvent(paintEvents[0]));

  function onRecordDetailsReady() {
    var updateCount = 0;

    panel.flameChart.detailsView.tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.PaintProfiler, true);
    var paintProfilerView = panel.flameChart.detailsView.lazyPaintProfilerView.paintProfilerView;
    TestRunner.addSniffer(paintProfilerView, 'update', onPaintProfilerUpdate, true);

    function onPaintProfilerUpdate() {
      // No snapshot, not a real update yet -- wait for another update!
      if (!paintProfilerView.snapshot)
        return;
      var logSize = paintProfilerView.log && paintProfilerView.log.length ? '>0' : '0';
      TestRunner.addResult('Paint ' + updateCount + ' log size: ' + logSize);
      if (updateCount++)
        TestRunner.completeTest();
      else
        panel.select(
            Timeline.TimelineSelection.fromTraceEvent(paintEvents[1]), Timeline.TimelineDetailsView.Tab.PaintProfiler);
    }
  }
})();

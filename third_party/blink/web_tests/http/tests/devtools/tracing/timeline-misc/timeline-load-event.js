// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the load event.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  UI.panels.timeline.disableCaptureJSProfileSetting.set(true);
  await PerformanceTestRunner.startTimeline();
  await TestRunner.reloadPagePromise();
  await TestRunner.evaluateInPagePromise(`
    function display() {
      return new Promise(resolve => {
        testRunner.setPopupBlockingEnabled(false);
        var popup = window.open("resources/hello.html");
        popup.onload = () => requestAnimationFrame(
            () => testRunner.updateAllLifecyclePhasesAndCompositeThen(resolve));
      });
    }
  `);
  await TestRunner.callFunctionInPageAsync('display');
  await PerformanceTestRunner.stopTimeline();

  TestRunner.addResult('Model records:');
  PerformanceTestRunner.printTimelineRecords('FrameStartedLoading');
  PerformanceTestRunner.printTimelineRecords('MarkDOMContent');
  PerformanceTestRunner.printTimelineRecords('MarkLoad');
  TestRunner.addResult('Timestamp records:');
  PerformanceTestRunner.printTimestampRecords('MarkDOMContent');
  PerformanceTestRunner.printTimestampRecords('MarkLoad');
  PerformanceTestRunner.printTimestampRecords('MarkFirstPaint');

  const markers = PerformanceTestRunner.timelineModel().timeMarkerEvents();
  markers.reduce((prev, current) => {
    TestRunner.assertGreaterOrEqual(current.startTime, prev.startTime,
        'Event divider timestamps should be monotonically non-decreasing');
    return current;
  });

  TestRunner.completeTest();
})();

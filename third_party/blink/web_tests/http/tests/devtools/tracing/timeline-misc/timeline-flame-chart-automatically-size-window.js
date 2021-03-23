// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the TimelineFlameChart automatically sized window.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var timeline = UI.panels.timeline;
  timeline._onModeChanged();
  timeline._flameChart._automaticallySizeWindow = true;

  function setWindowTimesHook(startTime, endTime) {
    if (startTime)
      TestRunner.addResult('time delta: ' + (endTime - startTime));
  }

  timeline._overviewPane.setWindowTimes = setWindowTimesHook;
  await PerformanceTestRunner.loadTimeline(PerformanceTestRunner.timelineData());
  TestRunner.completeTest();
})();

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the TimelineFlameChart automatically sized window.\n`);
  await TestRunner.loadLegacyModule('console');
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.showPanel('timeline');

  var timeline = UI.panels.timeline;
  timeline.onModeChanged();
  timeline.flameChart._automaticallySizeWindow = true;

  function setWindowTimesHook(startTime, endTime) {
    if (startTime)
      TestRunner.addResult('time delta: ' + (endTime - startTime));
  }

  timeline.overviewPane.setWindowTimes = setWindowTimesHook;
  await PerformanceTestRunner.loadTimeline(PerformanceTestRunner.timelineData());
  TestRunner.completeTest();
})();

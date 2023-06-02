// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests the Timeline window filter.`);
  TestRunner.addResult(`It applies different ranges to the OverviewGrid and expects that current view reflects the change.\n`);
  await TestRunner.loadLegacyModule('timeline');
  await TestRunner.loadLegacyModule("perf_ui");
  await TestRunner.showPanel('timeline');

  var timeline = UI.panels.timeline;
  var overviewPane = timeline.overviewPane;

  await PerformanceTestRunner.loadTimeline(PerformanceTestRunner.timelineData());

  overviewPane.update();
  TestRunner.addResult('OverviewPane:');
  overviewPane.overviewCalculator.setDisplayWidth(450);
  dumpDividers(overviewPane.overviewCalculator);
  TestRunner.addResult('');

  dumpFlameChartRecordsCountForRange(0, 1);
  dumpFlameChartRecordsCountForRange(0.25, 0.75);
  dumpFlameChartRecordsCountForRange(0.33, 0.66);

  overviewPane.overviewGrid.setWindow(0.1, 0.9);

  TestRunner.addResult('--------------------------------------------------------');
  const window = timeline.performanceModel.window();
  TestRunner.addResult(`time range = ${window.left} - ${window.right}`);
  TestRunner.completeTest();

  function dumpFlameChartRecordsCountForRange(windowLeft, windowRight) {
    var mainView = timeline.flameChart.mainFlameChart;
    mainView.muteAnimation = true;
    overviewPane.overviewGrid.setWindow(windowLeft, windowRight);
    mainView.update();
    TestRunner.addResult('range = ' + windowLeft + ' - ' + windowRight);
    const window = timeline.performanceModel.window();
    TestRunner.addResult(`time range = ${window.left} - ${window.right}`);
    TestRunner.addResult('');
  }

  function dumpDividers(calculator) {
    var times = PerfUI.TimelineGrid.calculateGridOffsets(calculator)
                    .offsets.map(offset => offset.time - calculator.zeroTime());
    TestRunner.addResult('divider offsets: [' + times.join(', ') + ']. We are expecting round numbers.');
  }
})();

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as Timeline from 'devtools/panels/timeline/timeline.js';

(async function() {

  await TestRunner.showPanel('timeline');
  TestRunner.addResult('Performance panel loaded.');

  const tabbedPane = Timeline.TimelinePanel.TimelinePanel.instance().flameChart.detailsView.tabbedPane;
  tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.EventLog);

  TestRunner.addResult('Loading a performance model.');
  const testData = [
    {
      'name': 'top level event name',
      'ts': 1000000,
      'ph': 'B',
      'tid': 1,
      'pid': 100,
      'cat': 'toplevel',
      'args': {'data': {'message': 'AAA'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1010000,
      'ph': 'B',
      'tid': 1,
      'pid': 100,
      'cat': 'toplevel',
      'args': {'data': {'message': 'BBB'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1020000,
      'ph': 'B',
      'tid': 1,
      'pid': 100,
      'cat': 'toplevel',
      'args': {'data': {'message': 'CCC'}}
    },
  ];
  const view = tabbedPane.visibleView;
  const traceEngineData = await PerformanceTestRunner.createTraceEngineDataFromEvents(testData);
  const mainThreadEvents = traceEngineData.Renderer.processes.get(100).threads.get(1).entries;

  view.setModelWithEvents(null, mainThreadEvents, traceEngineData);
  view.updateContents(Timeline.TimelineSelection.TimelineSelection.fromRange(
      // traceBounds are in microseconds, but fromRange expects milliseconds
      traceEngineData.Meta.traceBounds.min / 1000,
      traceEngineData.Meta.traceBounds.max / 1000
  ));

  TestRunner.addResult('Running aXe on the event log pane.');
  await AxeCoreTestRunner.runValidation(view.contentElement);
  TestRunner.completeTest();
})();

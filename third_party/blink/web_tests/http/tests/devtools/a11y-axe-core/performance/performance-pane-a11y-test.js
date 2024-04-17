// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as Timeline from 'devtools/panels/timeline/timeline.js';
(async function() {
  await TestRunner.showPanel('timeline');

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

  // create dummy data for test
  const traceEngineData = await PerformanceTestRunner.createTraceEngineDataFromEvents(testData)
  const mainThreadEvents = traceEngineData.Renderer.processes.get(100).threads.get(1).entries;

  const detailsView = Timeline.TimelinePanel.TimelinePanel.instance().flameChart.detailsView;

  async function testDetailsView() {
    TestRunner.addResult('Tests accessibility in performance Details view using the axe-core linter');

    detailsView.setModel(null, traceEngineData, mainThreadEvents);

    const tabbedPane = detailsView.tabbedPane;
    tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.Details);
    const detailsTab = tabbedPane.visibleView;

    await AxeCoreTestRunner.runValidation(detailsTab.element);
  }

  async function testViewWithName(tab) {
    TestRunner.addResult(`Tests accessibility in performance ${tab} view using the axe-core linter`);
    const tabbedPane = detailsView.tabbedPane;
    tabbedPane.selectTab(tab);
    const detailsTab = tabbedPane.visibleView;

    // update child views with the same test data
    detailsTab.setModelWithEvents(null, mainThreadEvents, traceEngineData);
    detailsTab.updateContents(Timeline.TimelineSelection.TimelineSelection.fromRange(
      // traceBounds are in microseconds, but fromRange expects milliseconds
      traceEngineData.Meta.traceBounds.min / 1000,
      traceEngineData.Meta.traceBounds.max / 1000
    ));

    await AxeCoreTestRunner.runValidation(detailsTab.element);
  }

  function testBottomUpView() {
    return testViewWithName(Timeline.TimelineDetailsView.Tab.BottomUp);
  }

  function testCallTreeView() {
    return testViewWithName(Timeline.TimelineDetailsView.Tab.CallTree);
  }

  TestRunner.runAsyncTestSuite([
    testDetailsView,
    testBottomUpView,
    testCallTreeView,
  ]);
})();

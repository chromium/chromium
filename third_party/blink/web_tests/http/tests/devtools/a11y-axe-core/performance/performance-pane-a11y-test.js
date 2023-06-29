// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';
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
  const model = await PerformanceTestRunner.createPerformanceModelWithEvents(testData);

  const detailsView = UI.panels.timeline.flameChart.detailsView;

  async function testDetailsView() {
    TestRunner.addResult('Tests accessibility in performance Details view using the axe-core linter');

    // Details pane gets data from the parent TimelineDetails view
    // model = SDK Performance Model
    // null = where we would pass in the new TraceEngine data, if we had it.
    detailsView.setModel(model, null, PerformanceTestRunner.mainTrackEvents());

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
    detailsTab.setModel(model, PerformanceTestRunner.mainTrack());
    detailsTab.updateContents(Timeline.TimelineSelection.fromRange(
        model.timelineModel().minimumRecordTime(),
        model.timelineModel().maximumRecordTime()));

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

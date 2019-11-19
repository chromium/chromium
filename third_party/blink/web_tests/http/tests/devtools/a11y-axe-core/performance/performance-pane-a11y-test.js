// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('performance_test_runner');
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
  const model =
      PerformanceTestRunner.createPerformanceModelWithEvents(testData);

  async function testDetailsView() {
    TestRunner.addResult(`Tests accessibility in performance Details view using the axe-core linter`);
    const detailsView = runtime.sharedInstance(Timeline.TimelineDetailsView);

    // Details pane gets data from the parent TimelineDetails view
    detailsView.setModel(model, PerformanceTestRunner.mainTrack());

    const tabbedPane = UI.panels.timeline._flameChart._detailsView._tabbedPane;
    tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.Details);
    const detailsTab = tabbedPane.visibleView;

    await AxeCoreTestRunner.runValidation(detailsTab.element);
  }

  async function testViewWithName(tab) {
    TestRunner.addResult(`Tests accessibility in performance ${tab} view using the axe-core linter`);
    const tabbedPane = UI.panels.timeline._flameChart._detailsView._tabbedPane;
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
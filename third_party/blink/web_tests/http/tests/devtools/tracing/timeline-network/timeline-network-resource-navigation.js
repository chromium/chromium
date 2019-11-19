// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests presence and order of tracing events for a browser navigation.\n`);
  await TestRunner.loadModule(`performance_test_runner`);
  await TestRunner.showPanel(`timeline`);
  await TestRunner.NetworkAgent.setCacheDisabled(true);

  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, TestRunner.pageLoaded);

  const panel = UI.panels.timeline;
  PerformanceTestRunner.runWhenTimelineIsReady(recordingStopped);
  panel._millisecondsToRecordAfterLoadEvent = 1;
  panel._recordReload();

  async function recordingStopped() {
    TestRunner.addResult('Recording stopped');
    const sendRequests = PerformanceTestRunner.timelineModel().networkRequests();
    for (const request of sendRequests) {
      TestRunner.addResult(`Number of events in request: ${request.children && request.children.length}`);
      let requestId = null;
      for (const event of request.children.sort((a,b) => a.startTime - b.startTime)) {
        if (requestId === null) {
          requestId = event.args.data.requestId;
        } else if (requestId !== event.args.data.requestId) {
          TestRunner.addResult(`Events did not have the same request id.`)
        }
        TestRunner.addResult(`${event.name} from thread ${event.thread._name}`);
      }
    }
    TestRunner.completeTest();
}
})();

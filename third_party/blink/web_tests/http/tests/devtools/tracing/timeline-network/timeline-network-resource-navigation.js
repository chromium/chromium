// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests presence and order of tracing events for a browser navigation.\n`);
  await TestRunner.showPanel(`timeline`);
  await TestRunner.NetworkAgent.setCacheDisabled(true);

  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, TestRunner.pageLoaded);

  const panel = UI.panels.timeline;
  PerformanceTestRunner.runWhenTimelineIsReady(recordingStopped);
  panel.millisecondsToRecordAfterLoadEvent = 1;
  panel.recordReload();

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
        TestRunner.addResult(`${event.name} from thread ${event.thread.name()}`);
      }
    }
    TestRunner.completeTest();
}
})();

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline events for WebSocket\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var ws = new WebSocket("ws://127.0.0.1:8880/simple");
          return new Promise((fulfill) => ws.onclose = fulfill);
      }
  `);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  await PerformanceTestRunner.printTimelineRecordsWithDetails('WebSocketCreate');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('WebSocketSendHandshakeRequest');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('WebSocketReceiveHandshakeResponse');
  await PerformanceTestRunner.printTimelineRecordsWithDetails('WebSocketDestroy');
  TestRunner.completeTest();
})();

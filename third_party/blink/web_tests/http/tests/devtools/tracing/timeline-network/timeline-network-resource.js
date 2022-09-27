// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a network resource load\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.NetworkAgent.setCacheDisabled(true);

  await PerformanceTestRunner.startTimeline();
  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/tracing/resources/hello.html');
  await TestRunner.evaluateInPagePromise(`
      var scriptUrl = "timeline-network-resource.js";

      function performActions()
      {
          var promise = new Promise((fulfill) => window.timelineNetworkResourceEvaluated = fulfill);
          var script = document.createElement("script");
          script.src = scriptUrl;
          document.body.appendChild(script);
          return promise;
      }
  `);

  var requestId;
  var scriptUrl = 'timeline-network-resource.js';

  await TestRunner.callFunctionInPageAsync('performActions');
  await PerformanceTestRunner.stopTimeline();

  const sendRequests = PerformanceTestRunner.mainTrackEvents().
      filter(e => e.name === TimelineModel.TimelineModel.RecordType.ResourceSendRequest);
  for (let event of sendRequests) {
    await printEvent(event);
    await printEventsWithId(event.args['data'].requestId);
  }
  TestRunner.completeTest();

  async function printEventsWithId(id) {
    var model = PerformanceTestRunner.timelineModel();
    for (const event of PerformanceTestRunner.mainTrackEvents()) {
      if (event.name !== TimelineModel.TimelineModel.RecordType.ResourceReceiveResponse &&
          event.name !== TimelineModel.TimelineModel.RecordType.ResourceFinish) {
        continue;
      }
      if (event.args['data'].requestId !== id)
        continue;
      await printEvent(event);
    }
  }

  async function printEvent(event) {
    TestRunner.addResult('');
    PerformanceTestRunner.printTraceEventProperties(event);
    TestRunner.addResult(
        `Text details for ${event.name}: ` + await Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(event));
  }
})();

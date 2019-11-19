// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a SendRequest, ReceiveResponse etc.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var image = new Image();
          image.onload = bar;
          // Use random urls to avoid caching.
          const random = Math.random();
          image.src = "resources/anImage.png?random=" + random;

          function bar()
          {
              var image = new Image();
              image.onload = function(event) { callback(); }  // do not pass event argument to the callback.
              image.src = "resources/anotherImage.png?random=" + random;
          }
          return promise;
      }
  `);

  UI.viewManager.showView('timeline');
  const panel = UI.panels.timeline;
  panel._disableCaptureJSProfileSetting.set(true);
  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');

  var recordTypes = TimelineModel.TimelineModel.RecordType;
  var typesToDump = new Set([
    recordTypes.ResourceSendRequest, recordTypes.ResourceReceiveResponse, recordTypes.ResourceReceivedData,
    recordTypes.ResourceFinish, recordTypes.EventDispatch, recordTypes.FunctionCall
  ]);
  const hasAlreadyDumpedReceivedDataFor = new Set();
  function dumpEvent(traceEvent, level) {
    // Ignore stray paint & rendering events for better stability.
    var categoryName = Timeline.TimelineUIUtils.eventStyle(traceEvent).category.name;
    if (categoryName !== 'loading' && categoryName !== 'scripting')
      return;
    if (traceEvent.name === 'ResourceReceivedData') {
      const requestId = traceEvent.args['data']['requestId'];
      // Dump only the first ResourceReceivedData for a request for stability.
      if (hasAlreadyDumpedReceivedDataFor.has(requestId))
        return;
      hasAlreadyDumpedReceivedDataFor.add(requestId);
    }

    // Here and below: pretend coalesced record are just not there, as coalescation is time dependent and, hence, not stable.
    // Filter out InjectedScript function call because they happen out of sync.
    if (typesToDump.has(traceEvent.name) && (traceEvent.name !== 'FunctionCall' || traceEvent.args['data']['url']))
      TestRunner.addResult('  '.repeat(level - 1) + traceEvent.name);
  }
  PerformanceTestRunner.walkTimelineEventTree(dumpEvent);
  TestRunner.completeTest();
})();

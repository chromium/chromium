// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that checks location resolving mechanics for TimerInstall TimerRemove and FunctionCall events with scriptId.
       It expects two FunctionCall for InjectedScript, two TimerInstall events, two FunctionCall events and one TimerRemove event to be logged with performActions.js script name and some line number.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('timeline');

  function performActions() {
    var callback;
    var promise = new Promise((fulfill) => callback = fulfill);
    var timerOne = setTimeout('1 + 1', 10);
    var timerTwo = setInterval(intervalTimerWork, 20);
    var iteration = 0;
    return promise;

    function intervalTimerWork() {
      if (++iteration < 2)
        return;
      clearInterval(timerTwo);
      callback();
    }
  }

  const source = performActions.toString() + '\n//# sourceURL=performActions.js';
  await new Promise(resolve => TestRunner.evaluateInPage(source, resolve));

  const linkifier = new Components.Linkifier();
  const recordTypes = new Set(['TimerInstall', 'TimerRemove', 'FunctionCall']);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');
  await PerformanceTestRunner.walkTimelineEventTree(formatter);
  TestRunner.completeTest();

  async function formatter(event) {
    if (!recordTypes.has(event.name))
      return;

    var detailsText = await Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(
        event, PerformanceTestRunner.timelineModel().targetByEvent(event));
    await TestRunner.waitForPendingLiveLocationUpdates();
    TestRunner.addResult('detailsTextContent for ' + event.name + ' event: \'' + detailsText + '\'');

    var details = await Timeline.TimelineUIUtils.buildDetailsNodeForTraceEvent(
        event, PerformanceTestRunner.timelineModel().targetByEvent(event), linkifier);
    await TestRunner.waitForPendingLiveLocationUpdates();
    if (!details)
      return;
    TestRunner.addResult(
        'details.textContent for ' + event.name + ' event: \'' + details.textContent.replace(/VM[\d]+/, 'VM') + '\'');
  }
})();

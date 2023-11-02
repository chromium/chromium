// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that checks whether the returned stackTrace of a timeline correctly formats the scriptId as a string.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('timeline');

  function performActions() {
    let callback;
    const promise = new Promise((fulfill) => callback = fulfill);
    const timer = setInterval(intervalTimerWork, 20);
    let iteration = 0;
    return promise;

    function intervalTimerWork() {
      if (++iteration < 2)
        return;
      clearInterval(timer);
      callback();
    }
  }

  const source = performActions.toString() + '\n//# sourceURL=performActions.js';
  await new Promise(resolve => TestRunner.evaluateInPage(source, resolve));

  const linkifier = new Components.Linkifier();
  const recordTypes = new Set(['TimerInstall', 'TimerRemove']);

  await PerformanceTestRunner.invokeAsyncWithTimeline('performActions');
  await PerformanceTestRunner.walkTimelineEventTree(formatter);
  TestRunner.completeTest();

  async function formatter(event) {
    if (!recordTypes.has(event.name))
      return;

    TestRunner.addResult('Stack trace for: ' + event.name)
    TestRunner.addResult(JSON.stringify(event.args.data.stackTrace));
  }
})();

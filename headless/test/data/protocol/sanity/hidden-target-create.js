// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(`Test creating hidden target`);

  const existingTargets = new Set();

  function testRunnerLog() {
    testRunner.log(
        arguments[0] ?? '', arguments[1] ?? '', testRunner.stabilizeNames,
        ['sessionId', 'targetId']);
  }

  const {result: targets} = await testRunner.browserP().Target.getTargets();
  for (const target of targets.targetInfos) {
    existingTargets.add(target.targetId);
  }

  testRunner.browserP().Target.onDetachedFromTarget((event) => {
    testRunnerLog(event, 'DetachedFromTarget');
  });
  testRunner.browserP().Target.onTargetDestroyed((event) => {
    testRunnerLog(event, 'TargetDestroyed');
  });
  /**
   * Filter out the targets that are already present in the existing targets set
   * keeping only the new targets. If the result is an error, return the error.
   */
  function filterTargets(result) {
    if (result.error) {
      return result.error;
    }
    return result.result.targetInfos
        .filter(item => !existingTargets.has(item.targetId))
        .map(item => ({targetId: item.targetId, url: item.url}));
  }

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const response = await target.attachToBrowserTarget();

  // Create a new session and create a hidden target via it.
  const newBrowserSession =
      new TestRunner.Session(testRunner, response.result.sessionId);

  testRunnerLog('Create hidden target');
  const {result: hiddenTarget} =
      await newBrowserSession.protocol.Target.createTarget({
        url: 'about:blank?HIDDEN=TARGET',
        hidden: true,
      });

  target.attachToTarget({targetId: hiddenTarget.targetId, flatten: true});
  const attachedToHiddenTargetEvent = await target.onceAttachedToTarget();
  testRunnerLog('Attached to the hidden target');

  const hiddenSession = new TestRunner.Session(
      testRunner, attachedToHiddenTargetEvent.params.sessionId);

  // Verify the hidden target's session is available.
  testRunnerLog(
      filterTargets(await hiddenSession.protocol.Target.getTargets()),
      `Filtered targets via hidden target's session: `);
  // Verify the hidden target is visible from the browser session.
  testRunnerLog(
      filterTargets(await testRunner.browserP().Target.getTargets()),
      `Filtered targets via browser session: `);

  testRunnerLog('Disconnect the parent session');
  newBrowserSession.disconnect();

  await testRunner.browserP().Target.onceTargetDestroyed();

  // Verify the hidden target's session is closed.
  testRunnerLog(
      filterTargets(await hiddenSession.protocol.Target.getTargets()),
      `Expected error response: `);

  // Verify the hidden target is not present anymore.
  testRunnerLog(
      filterTargets(await testRunner.browserP().Target.getTargets()),
      `Filtered targets via browser session: `);

  testRunner.completeTest();
})

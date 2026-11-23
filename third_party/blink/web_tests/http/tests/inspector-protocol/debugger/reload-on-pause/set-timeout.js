// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests "reload" from within inspector window while on pause.`);

  await dp.Page.enable();
  await dp.Debugger.enable();
  await session.evaluate(`
    function testFunction() {
      debugger;
    }
  `);

  const pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0);');
  testRunner.log('Set timer for test function.');

  const paused = await pausedPromise;
  if (paused.params.callFrames[0]?.functionName !== 'testFunction') {
    testRunner.fail(`Expected pause in testFunction, got ${
        paused.params.callFrames[0]?.functionName}`);
  }
  testRunner.log('Script execution paused.');

  dp.Debugger.onPaused(msg => {
    testRunner.fail(`Unexpected pause after reload: ${
        msg.params.callFrames[0]?.functionName}`);
  });
  await Promise.all([dp.Page.reload(), dp.Page.onceLoadEventFired()]);
  if (await session.evaluate('typeof testFunction') !== 'undefined') {
    testRunner.fail('Expected page to reload into a fresh context.');
  }
  testRunner.log('Page reloaded.');

  testRunner.completeTest();
});

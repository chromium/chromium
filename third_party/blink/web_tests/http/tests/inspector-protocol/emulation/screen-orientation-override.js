// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test screen orientation override.');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await session.evaluate('testRunner.disableMockScreenOrientation()');

  async function injectLogger() {
    await session.evaluate(`
      var windowOrientationChangeEvent = false;
      var screenOrientationChangeEvent = false;

      window.addEventListener("orientationchange", function() { windowOrientationChangeEvent = true; maybeLog(); });
      screen.orientation.addEventListener("change", function() { screenOrientationChangeEvent = true; maybeLog(); });

      function dump()
      {
          return "angle: " + screen.orientation.angle + "; type: " + screen.orientation.type;
      }

      function maybeLog()
      {
          if (windowOrientationChangeEvent && screenOrientationChangeEvent) {
              windowOrientationChangeEvent = false;
              screenOrientationChangeEvent = false;
              console.log(dump());
          }
      }
    `);
  }

  await injectLogger();

  function addDumpResult() {
    return dp.Runtime.onceConsoleAPICalled().then(event => {
      const result = event.params.args[0].value;
      testRunner.log(result);
    });
  }

  async function testOverride(angle, orientation) {
    const dumpPromise = addDumpResult();
    await dp.Emulation.setDeviceMetricsOverride({
      width: 0,
      height: 0,
      deviceScaleFactor: 0,
      mobile: true,
      fitWindow: false,
      screenOrientation: {type: orientation, angle: angle}
    });
    await dumpPromise;
  }

  async function testError(angle, orientation) {
    const result = await dp.Emulation.setDeviceMetricsOverride({
      width: 0,
      height: 0,
      deviceScaleFactor: 0,
      mobile: true,
      fitWindow: false,
      screenOrientation: {type: orientation, angle: angle}
    });
    if (result.error)
      testRunner.log(`Caught expected error: ${result.error.message}`);
    else
      testRunner.log('FAIL: expected an error but did not get one.');
  }

  const original = await session.evaluate('dump()');

  testRunner.log('Initial state: ' + original);

  await testError(-1, 'portraitPrimary');
  await testError(360, 'portraitPrimary');
  await testError(120, 'wrongType');

  await testOverride(180, 'portraitSecondary');
  await testOverride(90, 'landscapePrimary');
  await testOverride(0, 'portraitPrimary');

  testRunner.log('Restoring after reload');

  const load = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await load;
  await injectLogger();
  await testOverride(270, 'landscapeSecondary');

  await dp.Emulation.clearDeviceMetricsOverride();
  testRunner.log(await session.evaluate('dump()'));

  testRunner.completeTest();
})

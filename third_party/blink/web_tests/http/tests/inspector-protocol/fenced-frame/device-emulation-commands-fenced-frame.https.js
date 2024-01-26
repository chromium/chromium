(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that device emulation commands from fenced frames.');

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  const ffdp = session.createChild(sessionId).protocol;

  await session.protocol.Emulation.clearDeviceMetricsOverride();
  const default_width = await session.evaluate(`screen.width`);
  // Fenced Frames can't override device metrics.
  testRunner.log('Trying to call Emulation.setDeviceMetricsOverride')
  const result1 = await ffdp.Emulation.setDeviceMetricsOverride({
    width: default_width * 2,
    height: 600,
    deviceScaleFactor: 1.0,
    mobile: true,
    screenOrientation: {type: 'landscapeSecondary', angle: 270},
  });
  testRunner.log(result1.error ? 'PASS: ' + result1.error.message : 'FAIL');
  testRunner.log(
      default_width == await session.evaluate(`screen.width`) ?
          'PASS: screen.width isn\'t changed.' :
          'FAIL: screen width is changed');

  // Override device metrics from the outermost main frame.
  await session.protocol.Emulation.setDeviceMetricsOverride({
    width: default_width * 2,
    height: 600,
    deviceScaleFactor: 1.0,
    mobile: true,
    screenOrientation: {type: 'landscapeSecondary', angle: 270},
  });
  const overridden_width = await session.evaluate(`screen.width`);
  // Fenced Frames can't clear overridden device metrics.
  testRunner.log('Trying to call Emulation.clearDeviceMetricsOverride')
  const result2 = await ffdp.Emulation.clearDeviceMetricsOverride();
  testRunner.log(result2.error ? 'PASS: ' + result2.error.message : 'FAIL');
  testRunner.log(
      overridden_width == await session.evaluate(`screen.width`) ?
          'PASS: screen.width isn\'t changed.' :
          'FAIL: screen width is changed');

  testRunner.completeTest();
})

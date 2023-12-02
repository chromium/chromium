(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
    `Test the Device Posture API default javascript API.`);

  testRunner.log('Initial Device Posture Type: ' + await session.evaluate('navigator.devicePosture.type'));
  await dp.Emulation.setDeviceMetricsOverride({
    width: 0,
    height: 0,
    deviceScaleFactor: 2.5,
    mobile: true,
    scale: 1.,
    devicePosture: {type : 'folded'}
  });
  testRunner.log('Updated Device Posture Type: ' + await session.evaluate('navigator.devicePosture.type'));
  testRunner.completeTest();
})

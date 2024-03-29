(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Test the Device Posture API default javascript API.`);

  testRunner.log(
      'Initial Device Posture Type: ' +
      await session.evaluate('navigator.devicePosture.type'));
  await dp.Emulation.setDevicePostureOverride({posture: {type: 'folded'}});
  testRunner.log(
      'Updated Device Posture Type: ' +
      await session.evaluate('navigator.devicePosture.type'));
  testRunner.completeTest();
})

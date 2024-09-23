(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Test the Device Posture API change event handler.`);

  function waitForChangeAndGetPosture() {
    return session.evaluateAsync(`
      new Promise(resolve => {
        navigator.devicePosture.addEventListener(
          'change',
          () => { resolve(navigator.devicePosture.type); },
          { once: true }
        );
      });
    `);
  }

  testRunner.log(
      'Initial Device Posture Type: ' +
      await session.evaluate('navigator.devicePosture.type'));
  let posture = waitForChangeAndGetPosture();
  await dp.Emulation.setDevicePostureOverride({posture: {type: 'folded'}});
  testRunner.log(
      `Updated Device Posture Type from addeventlistener change event: ${
          await posture}`);
  posture = waitForChangeAndGetPosture();
  await dp.Emulation.clearDevicePostureOverride();
  testRunner.log(
      `Updated Device Posture Type from addeventlistener change event: ${
          await posture}`);
  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Test the Device Posture API change event handler.`);

  testRunner.log(
      'Initial Device Posture Type: ' +
      await session.evaluate('navigator.devicePosture.type'));

  testRunner.log(`matchMedia('(device-posture: continuous)').matches :`);
  testRunner.log(await session.evaluate(
      `(window.matchMedia('(device-posture: continuous)').matches)`));

  testRunner.log(`matchMedia('(device-posture: folded)').matches :`);
  testRunner.log(await session.evaluate(`
    const foldedMQL = window.matchMedia('(device-posture: folded)');
    foldedMQL.matches;
  `));

  const mediaQueryPostureChanged = session.evaluateAsync(`
    new Promise(resolve => {
      foldedMQL.addEventListener(
        'change',
        () => { resolve(foldedMQL.matches); },
        { once: true }
      );
    })
  `);
  await dp.Emulation.setDevicePostureOverride({posture: {type: 'folded'}});
  testRunner.log(`Media Query change event folded matches: ${
      await mediaQueryPostureChanged}`);
  testRunner.completeTest();
})

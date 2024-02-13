(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests DeviceOrientation.SetDeviceOrientationOverride() basic usage');

  async function waitAndLogEvent() {
    testRunner.log(await session.evaluateAsync(`
      new Promise(resolve => {
        window.addEventListener('deviceorientation', event => {
          resolve([event.alpha, event.beta, event.gamma, event.absolute]);
        }, { once: true });
      })
    `));
  }

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.expectedSuccess(
      'Overriding with first set of values',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 3}));

  await waitAndLogEvent();

  testRunner.expectedSuccess(
      'Overriding with new set of values',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 42, beta: 34, gamma: -90}));

  await waitAndLogEvent();

  testRunner.completeTest();
})

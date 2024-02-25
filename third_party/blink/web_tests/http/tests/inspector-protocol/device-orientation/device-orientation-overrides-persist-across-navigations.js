(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that device orientation overrides persist across navigations');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.expectedSuccess(
      'Created sensor override',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 3}));

  testRunner.log('\nNavigating to a different URL');
  await session.navigate('./resources/simple.html');

  testRunner.log('\nAdding event listener and waiting for orientation data');

  testRunner.log(await session.evaluateAsync(`
      new Promise(resolve => {
        window.addEventListener('deviceorientation', event => {
          resolve([event.alpha, event.beta, event.gamma, event.absolute]);
        }, { once: true });
      })
  `));

  testRunner.completeTest();
})

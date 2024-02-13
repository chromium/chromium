(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that device orientation overrides affect event listeners in iframes');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.expectedSuccess(
      'Created sensor override',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 3}));

  testRunner.log(
      '\nAdding event listener in iframe and waiting for orientation data');

  testRunner.log(await session.evaluateAsync(`
      new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.src = '/resources/blank.html';
        iframe.onload = () => {
          iframe.contentWindow.addEventListener('deviceorientation', event => {
            resolve([event.alpha, event.beta, event.gamma, event.absolute]);
          }, { once: true });
        }
        document.body.appendChild(iframe);
      })
  `));

  testRunner.completeTest();
})

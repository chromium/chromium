(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that permissions could be granted`);

  // Reset all permissions initially.
  await dp.Browser.resetPermissions();

  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  // Start listening for geolocation changes.
  await session.evaluateAsync(async () => {
    window.messages = [];
    window.subscriptionChanges = [];
    const result = await navigator.permissions.query({name: 'geolocation'});
    window.subscriptionChanges.push(`INITIAL 'geolocation': ${result.state}`);
    result.onchange = () => window.subscriptionChanges.push(`CHANGED 'geolocation': ${result.state}`);
  });

  await grant('geolocation');
  await waitPermission({name: 'geolocation'}, 'granted');

  await grant('audioCapture');
  await Promise.all([
    waitPermission({name: 'geolocation'}, 'denied'),
    waitPermission({name: 'microphone'}, 'granted'),
  ]);

  await grant('geolocation', 'audioCapture', 'videoCapturePanTiltZoom');
  await Promise.all([
    waitPermission({name: 'geolocation'}, 'granted'),
    waitPermission({name: 'microphone'}, 'granted'),
    waitPermission({name: 'camera', panTiltZoom: true}, 'granted'),
    waitPermission({name: 'camera', panTiltZoom: false}, 'granted'),
    waitPermission({name: 'camera'}, 'granted'),
  ]);

  await grant('eee');

  testRunner.log('- Resetting all permissions');
  await dp.Browser.resetPermissions();
  await Promise.all([
    waitPermission({name: 'geolocation'}, 'denied'),
    waitPermission({name: 'microphone'}, 'denied'),
    waitPermission({name: 'camera', panTiltZoom: true}, 'denied'),
    waitPermission({name: 'camera', panTiltZoom: false}, 'denied'),
    waitPermission({name: 'camera'}, 'denied'),
  ]);

  testRunner.log(await session.evaluate(() => window.subscriptionChanges));
  testRunner.log(await session.evaluate(() => window.messages));

  testRunner.completeTest();

  async function grant(...permissions) {
    const response = await dp.Browser.grantPermissions({
      origin: 'http://devtools.test:8000',
      permissions
    });
    if (response.error)
      testRunner.log('- Failed to grant: ' + JSON.stringify(permissions) + '  error: ' + response.error.message);
    else
      testRunner.log('- Granted: ' + JSON.stringify(permissions));
  }

  async function waitPermission(descriptor, state) {
    await session.evaluateAsync(async (descriptor, state) => {
      const result = await navigator.permissions.query(descriptor);
      if (result.state && result.state === state)
        window.messages.push(`${JSON.stringify(descriptor)}: ${result.state}`);
      else
        window.messages.push(`Failed to set ${permission} to state: ${state}`);
    }, descriptor, state);
  }
})


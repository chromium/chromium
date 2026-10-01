(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageURL = testRunner.url('../resources/inspector-protocol-page.html');
  const prerenderURL = testRunner.url('../resources/test-page.html');

  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      'Tests that emulation overrides are cleared after prerender activation.');

  const tp = tabTargetSession.protocol;

  const childTargetManager =
      testRunner.createChildTargetManagerFor(tabTargetSession);
  await childTargetManager.startAutoAttach();

  const session1 = childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp1 = session1.protocol;
  await dp1.Page.enable();
  await dp1.Preload.enable();

  await session1.navigate(pageURL);

  await dp1.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['geolocation'],
  });

  await dp1.Emulation.setGeolocationOverride({
    latitude: 56.83,
    longitude: 60.63,
    accuracy: 1.23,
  });

  await dp1.Emulation.setDeviceMetricsOverride({
    width: 600,
    height: 400,
    deviceScaleFactor: 1,
    mobile: false,
  });

  await session1.evaluate(`
    window.__touchReceived = false;
    window.addEventListener('touchstart', () => window.__touchReceived = true);
  `);
  await dp1.Emulation.setEmitTouchEventsForMouse({
    enabled: true,
    configuration: 'mobile',
  });
  await dp1.Input.emulateTouchFromMouseEvent({
    type: 'mousePressed',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 100,
  });
  await dp1.Input.emulateTouchFromMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 100,
  });

  async function logGeolocationData(activeSession, label) {
    const result = await activeSession.evaluateAsync(`
      new Promise(
        resolve => window.navigator.geolocation.getCurrentPosition(
          position => resolve(position.coords.toJSON()),
          error => resolve({code: error.code, message: error.message}),
          {timeout: 200}
      ))`);
    testRunner.log(result, label + ': ');
  }

  testRunner.log('\nGet emulation state on primary page with override');
  await logGeolocationData(session1, 'Geolocation data');
  testRunner.log('Dimensions: ' + await session1.evaluate('window.innerWidth + "x" + window.innerHeight'));
  testRunner.log('Touch event received: ' + await session1.evaluate('window.__touchReceived'));

  async function waitForPrerenderStatusUpdated(dp, expectedStatus) {
    return dp.Preload.oncePrerenderStatusUpdated(
        e => e.params.status === expectedStatus);
  }

  const readyPromise = waitForPrerenderStatusUpdated(dp1, 'Ready');
  await session1.evaluate(`
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = JSON.stringify({
      prerender: [{
        source: 'list',
        urls: ['${prerenderURL}']
      }]
    });
    document.body.appendChild(script);
  `);
  await readyPromise;

  const session2 = childTargetManager.findAttachedSessionPrerender();
  const dp2 = session2.protocol;
  await dp2.Preload.enable();

  const detachedPromise = tp.Target.onceDetachedFromTarget();
  const successPromise = waitForPrerenderStatusUpdated(dp2, 'Success');

  await session1.evaluate(`location.href = '${prerenderURL}'`);
  await Promise.all([detachedPromise, successPromise]);

  await dp2.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['geolocation'],
  });

  testRunner.log('\nGet emulation state in activated page');
  await logGeolocationData(session2, 'Geolocation data');
  testRunner.log('Dimensions: ' + await session2.evaluate('window.innerWidth + "x" + window.innerHeight'));

  await session2.evaluate(`
    window.__touchReceived = false;
    window.addEventListener('touchstart', () => window.__touchReceived = true);
  `);
  await dp2.Input.emulateTouchFromMouseEvent({
    type: 'mousePressed',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 100,
  });
  await dp2.Input.emulateTouchFromMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    clickCount: 1,
    x: 100,
    y: 100,
  });
  testRunner.log('Touch event received: ' + await session2.evaluate('window.__touchReceived'));

  testRunner.completeTest();
});

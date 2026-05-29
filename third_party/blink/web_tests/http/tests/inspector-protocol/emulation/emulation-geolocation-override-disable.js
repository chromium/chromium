(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that geolocation override is cleared on session disconnect.');

  await dp.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['geolocation'],
  });

  async function logGeolocationData(activeSession) {
    const result = await activeSession.evaluateAsync(`
      new Promise(
        resolve => window.navigator.geolocation.getCurrentPosition(
          position => resolve(position.coords.toJSON()),
          error => resolve({code: error.code, message: error.message}),
          {timeout: 200}
      ))`);
    testRunner.log(result, 'Geolocation data: ');
  }

  testRunner.log('\nGet original geolocation data');
  await logGeolocationData(session);

  testRunner.log('\nSet required geolocation override fields');
  await dp.Emulation.setGeolocationOverride({
    latitude: 56.83,
    longitude: 60.63,
    accuracy: 1.23,
  });
  await logGeolocationData(session);

  testRunner.log('\nDisconnect session');
  await session.disconnect();

  const nextSession = await page.createSession();
  await nextSession.protocol.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['geolocation'],
  });
  await logGeolocationData(nextSession);

  testRunner.completeTest();
});

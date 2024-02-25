(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setSensorOverrideEnabled handles metadata');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  // Invalid frequencies cause an error to be reported.
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled({
    enabled: true,
    type: 'gyroscope',
    metadata: {minimumFrequency: 42, maximumFrequency: 34}
  }));

  // Test that metadata information is ignored if `enabled` is false.
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled({
    enabled: false,
    type: 'gyroscope',
    metadata: {minimumFrequency: 42, maximumFrequency: 34}
  }));

  // `available` set to false causes sensor.start() to fail.
  await dp.Emulation.setSensorOverrideEnabled(
      {enabled: true, type: 'gyroscope', metadata: {available: false}});
  const errorName = await session.evaluateAsync(`
    const sensor = new Gyroscope();
    new Promise(resolve => {
      sensor.onerror = e => {
        resolve(e.error.name);
      }
      sensor.start();
    })
  `);
  testRunner.log(`sensor.start() failed with ${errorName}`);

  testRunner.completeTest();
})

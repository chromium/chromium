(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests getOverriddenSensorInformation delivers correct information');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  testRunner.log(
      'Calling getOverriddenSensorInformation() with a sensor type that is not overridden');
  testRunner.log(
      await dp.Emulation.getOverriddenSensorInformation({type: 'gravity'}));

  testRunner.log('\nOverriding sensor type with sane frequencies');
  await dp.Emulation.setSensorOverrideEnabled({
    enabled: true,
    type: 'gravity',
    metadata: {minimumFrequency: 0.5, maximumFrequency: 4}
  });

  testRunner.log(
      '\nCalling getOverriddenSensorInformation() on a stopped sensor');
  testRunner.log(
      await dp.Emulation.getOverriddenSensorInformation({type: 'gravity'}));

  testRunner.log('\nStarting sensor with a frequency that is too high');
  await session.evaluateAsync(`
    let sensor = new GravitySensor({ frequency: 9 });
    new Promise(resolve => {
      sensor.onactivate = resolve;
      sensor.start();
    })
  `);
  testRunner.log(
      await dp.Emulation.getOverriddenSensorInformation({type: 'gravity'}));

  testRunner.log('\nStarting sensor with a frequency that is too low');
  await session.evaluateAsync(`
    sensor.stop();
    sensor = new GravitySensor({ frequency: 0.3 });
    new Promise(resolve => {
      sensor.onactivate = resolve;
      sensor.start();
    })
  `);
  testRunner.log(
      await dp.Emulation.getOverriddenSensorInformation({type: 'gravity'}));

  testRunner.log('\nStopping sensor and retrieving information');
  await session.evaluateAsync('sensor.stop()');
  testRunner.log(
      await dp.Emulation.getOverriddenSensorInformation({type: 'gravity'}));

  testRunner.completeTest();
})

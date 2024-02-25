(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that sensor overrides persist across navigations');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.expectedSuccess(
      'Created gyroscope virtual sensor',
      await dp.Emulation.setSensorOverrideEnabled({
        type: 'gyroscope',
        enabled: true,
        metadata: {minimumFrequency: 34, maximumFrequency: 34}
      }));

  testRunner.log('Navigating to a different URL');
  await session.navigate('/resources/blank.html');

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  testRunner.log('Creating Gyroscope object in script');
  await session.evaluateAsync(`
      new Promise(resolve => {
        const sensor = new Gyroscope;
        sensor.onactivate = resolve;
        sensor.start();
      })
   `);

  testRunner.log('Verifying sensor override information');
  const info =
      await dp.Emulation.getOverriddenSensorInformation({type: 'gyroscope'});
  testRunner.expectedSuccess('\'gyroscope\' is being overridden', info);
  // Check the requested sampling frequency to verify that the sensor is active
  // and that it has the same metadata requested before the navigation.
  if (info.result.requestedSamplingFrequency !== 34) {
    testRunner.fail(`Expected requestedSamplingFrequency == 34, got ${
        info.result.requestedSamplingFrequency}`);
  }

  testRunner.completeTest();
})

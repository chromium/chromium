(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that each Emulation handler keeps track of its sensor overrides');

  function createAndStartSensor(session, sensorName) {
    // Which session to use is irrelevant because all sensor requests go
    // through the same WebContentsSensorProviderProxy, so we just use
    // |session| for all calls.
    return session.evaluateAsync(`
      new Promise(resolve => {
        const sensor = new ${sensorName};
        sensor.onactivate = resolve;
        sensor.start();
      })
   `);
  }

  async function assertSensorIsOverridden(dp, type) {
    const info = await dp.Emulation.getOverriddenSensorInformation({type});
    testRunner.expectedSuccess(`'${type}' is being overridden`, info);

    if (info.result.requestedSamplingFrequency <= 0) {
      testRunner.fail(`Expected requestedSamplingFrequency > 0, got ${
          info.result.requestedSamplingFrequency}`);
    }
  }

  async function assertSensorIsNotOverridden(dp, type) {
    const info = await dp.Emulation.getOverriddenSensorInformation({type});
    testRunner.expectedError(`'${type}' is not being overridden`, info);
  }

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  // Create more sessions to test that overrides are tracked separately.
  const session2 = await page.createSession();
  const dp2 = session2.protocol;
  const session3 = await page.createSession();

  testRunner.log('\nCreating virtual sensors');

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'gyroscope', enabled: true});
  await dp2.Emulation.setSensorOverrideEnabled(
      {type: 'accelerometer', enabled: true});
  await dp2.Emulation.setSensorOverrideEnabled(
      {type: 'gravity', enabled: true});
  await createAndStartSensor(session2, 'Accelerometer');
  await createAndStartSensor(session2, 'GravitySensor');
  await createAndStartSensor(session, 'Gyroscope');

  testRunner.expectedError(
      'Attempting to override sensor already overridden in other session fails',
      await dp.Emulation.setSensorOverrideEnabled(
          {type: 'accelerometer', enabled: true}));

  // Disconnecting this session should not impact any of the virtual sensors
  // created by other handlers.
  testRunner.log('\nDisconnecting |session3|');
  await session3.disconnect();

  await assertSensorIsOverridden(dp2, 'accelerometer');
  await assertSensorIsOverridden(dp2, 'gravity');
  await assertSensorIsOverridden(dp, 'gyroscope');

  testRunner.log('\nRemoving GravitySensor virtual sensor');

  await dp2.Emulation.setSensorOverrideEnabled(
      {type: 'gravity', enabled: false});
  testRunner.expectedError(
      `'gravity' is not being overridden`,
      await dp.Emulation.getOverriddenSensorInformation({type: 'gravity'}));
  await assertSensorIsOverridden(dp2, 'accelerometer');
  await assertSensorIsOverridden(dp, 'gyroscope');

  testRunner.log('\nDisconnecting |session2|');
  await session2.disconnect();

  await assertSensorIsNotOverridden(dp2, 'accelerometer');
  await assertSensorIsNotOverridden(dp2, 'gravity');
  await assertSensorIsOverridden(dp, 'gyroscope');

  testRunner.log('\nCreating a new virtual accelerometer');
  testRunner.expectedSuccess(
      'setSensorOverrideEnabled() does not fail this time',
      await dp.Emulation.setSensorOverrideEnabled(
          {type: 'accelerometer', enabled: true}));
  await createAndStartSensor(session, 'Accelerometer');

  testRunner.log('\nDisconnection session');
  await session.disconnect();

  await assertSensorIsNotOverridden(dp, 'accelerometer');
  await assertSensorIsNotOverridden(dp2, 'gravity');
  await assertSensorIsNotOverridden(dp, 'gyroscope');

  testRunner.completeTest();
})

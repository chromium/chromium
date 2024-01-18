(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests removing a virtual sensor in use delivers an error');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'gyroscope', enabled: true});

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  // We need to call Sensor.start() first and wait for it to connect to the
  // virtual sensor before calling setSensorOverrideEnabled() to deactivate the
  // virtual sensor.
  await session.evaluateAsync(`
    const sensor = new Gyroscope();
    const errorPromise = new Promise(resolve => {
      sensor.onerror = (evt) => {
        resolve(evt.error.message);
      }
    });
    new Promise(resolve => {
      sensor.onactivate = () => {
        resolve();
      }
      sensor.start();
    })
  `);
  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'gyroscope', enabled: false});
  testRunner.log(
      `error message: ${await session.evaluateAsync('errorPromise')}`);

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setSensorOverrideReadings delivers updates');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'linear-acceleration', enabled: true});

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  // We need to call Sensor.start() first and wait for it to connect to the
  // virtual sensor before calling setSensorOverrideEnabled() to deactivate the
  // virtual sensor.
  await session.evaluateAsync(`
    const sensor = new LinearAccelerationSensor();
    new Promise(resolve => {
      sensor.onactivate = () => {
        resolve();
      }
      sensor.start();
    })
  `);

  testRunner.log(await dp.Emulation.setSensorOverrideReadings(
      {type: 'linear-acceleration', reading: {xyz: {x: 1, y: 30, z: 4}}}));

  testRunner.log(await session.evaluateAsync(`
    new Promise(resolve => {
      sensor.addEventListener('reading', () => {
        resolve([sensor.x, sensor.y, sensor.z]);
      }, { once: true });
    })
  `));

  testRunner.log(await dp.Emulation.setSensorOverrideReadings(
      {type: 'linear-acceleration', reading: {xyz: {x: -5, y: 42, z: 34}}}));

  testRunner.log(await session.evaluateAsync(`
    new Promise(resolve => {
      sensor.addEventListener('reading', () => {
        resolve([sensor.x, sensor.y, sensor.z]);
      }, { once: true });
    })
  `));

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests removing a non-existent virtual sensor is a no-op');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  // SensorProxy::ShouldSuspendUpdates() calls FocusController::IsFocused(), so
  // we need to emulate focus on the page.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'gyroscope', enabled: false});

  testRunner.completeTest();
})

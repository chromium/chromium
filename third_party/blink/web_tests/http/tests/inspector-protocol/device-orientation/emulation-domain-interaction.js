(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that the DeviceOrientation and Emulationd domains cannot override the same sensor at the same time');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.log('\nCreating virtual sensor via Emulation domain');

  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'relative-orientation', enabled: true});

  testRunner.expectedError(
      'Attempting to create the same virtual sensor via DeviceOrientation domain fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 3}));

  testRunner.log('\nRemoving virtual sensor created via Emulation domain');

  await dp.Emulation.setSensorOverrideEnabled(
      {type: 'relative-orientation', enabled: false});

  testRunner.log('\nCreating virtual sensor via DeviceOrientation domain');
  testRunner.expectedSuccess(
      'DeviceOrientation.SetDeviceOrientationOverride() works',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 3}));

  testRunner.log('\nAttempting to create sensor via Emulation domain');
  testRunner.expectedError(
      'Virtual sensor is already overridden',
      await dp.Emulation.setSensorOverrideEnabled(
          {type: 'relative-orientation', enabled: true}));

  // This is supposed to fail because the virtual sensor is not being
  // overridden by the Emulation domain handler.
  testRunner.expectedError(
      'Calling Emulation.getVirtualSensorInformation fails',
      await dp.Emulation.getVirtualSensorInformation(
          {type: 'relative-orientation'}));

  testRunner.completeTest();
})

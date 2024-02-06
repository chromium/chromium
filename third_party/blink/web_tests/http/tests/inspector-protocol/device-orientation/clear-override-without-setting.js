(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that ClearDeviceOrientationOverride() works without SetDeviceOrientationOverride()');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.expectedSuccess(
      'ClearDeviceOrientationOverride() works',
      await dp.DeviceOrientation.clearDeviceOrientationOverride());

  testRunner.completeTest();
})

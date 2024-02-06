(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that values passed to SetDeviceOrientationOverride() are validated');

  await dp.Browser.grantPermissions(
      {origin: location.origin, permissions: ['sensors']});

  testRunner.log('\nTesting alpha in range [0, 360)');
  testRunner.expectedError(
      'alpha < 0 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: -0.1, beta: 2, gamma: 3}));
  testRunner.expectedError(
      'alpha == 360 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 360, beta: 2, gamma: 3}));
  testRunner.expectedError(
      'alpha > 360 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 361, beta: 2, gamma: 3}));

  testRunner.log('\nTesting beta in range [-180, 180)');
  testRunner.expectedError(
      'beta < -180 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: -180.1, gamma: 3}));
  testRunner.expectedError(
      'beta == 180 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 180, gamma: 3}));
  testRunner.expectedError(
      'beta > 180 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 181, gamma: 3}));

  testRunner.log('\nTesting gamma in range [-90, 90)');
  testRunner.expectedError(
      'gamma < -90 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: -90.1}));
  testRunner.expectedError(
      'gamma == 90 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 90}));
  testRunner.expectedError(
      'gamma > 90 fails',
      await dp.DeviceOrientation.setDeviceOrientationOverride(
          {alpha: 1, beta: 2, gamma: 90.1}));

  testRunner.completeTest();
})

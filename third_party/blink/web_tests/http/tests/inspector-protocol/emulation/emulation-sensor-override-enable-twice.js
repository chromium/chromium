(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setSensorOverrideEnabled fails if enabling the same sensor twice');

  testRunner.log('Enabling sensor override');
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled(
      {enabled: true, type: 'gyroscope'}));

  // Test that the call fails regardless of changes to the `metadata` field.
  testRunner.log('\nAttempting to enable an already enabled sensor');
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled(
      {enabled: true, type: 'gyroscope'}));
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled(
      {enabled: true, type: 'gyroscope', metadata: {minimumFrequency: 2}}));

  testRunner.log('\nRemoving override and enabling it again');
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled(
      {enabled: false, type: 'gyroscope'}));
  testRunner.log(await dp.Emulation.setSensorOverrideEnabled(
      {enabled: true, type: 'gyroscope', metadata: {minimumFrequency: 2}}));

  testRunner.completeTest();
})

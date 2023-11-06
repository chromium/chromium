(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setSensorOverrideReadings fails if sensor is not overridden');

  testRunner.log(await dp.Emulation.setSensorOverrideReadings(
      {type: 'gyroscope', reading: {xyz: {x: 1, y: 2, z: 3}}}));

  testRunner.completeTest();
})

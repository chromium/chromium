(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setSensorOverrideReadings validates the readings format');

  testRunner.log(await dp.Emulation.setSensorOverrideReadings(
      {type: 'gyroscope', reading: {quaternion: {x: 1, y: 2, z: 3, w: 4}}}));

  testRunner.log(await dp.Emulation.setSensorOverrideReadings(
      {type: 'ambient-light', reading: {xyz: {x: 1, y: 2, z: 3}}}));

  testRunner.log(await dp.Emulation.setSensorOverrideReadings(
      {type: 'relative-orientation', reading: {single: {value: 1}}}));

  testRunner.completeTest();
})

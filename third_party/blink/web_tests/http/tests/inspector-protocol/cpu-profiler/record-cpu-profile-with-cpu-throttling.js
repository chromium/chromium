(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Test that the profiler can record a profile with cpu throttling enabled.');

  await dp.Emulation.setCPUThrottlingRate({rate: 4});
  await dp.Profiler.enable();
  await dp.Profiler.start();

  // Run some JS for at least a second to give a chance for the two signals to
  // overlap.
  await session.evaluate(`
    let count = 0;
    const limit = 1e6;
    let i = 0;
    let target = new Date();
    target.setSeconds(target.getSeconds() + 1);
    let time = new Date();
    while (i < limit && time < target) {
      count += i;
      i++;
      time = new Date();
    }
    window.count = count;
  `);

  await dp.Profiler.stop();

  testRunner.completeTest();
})

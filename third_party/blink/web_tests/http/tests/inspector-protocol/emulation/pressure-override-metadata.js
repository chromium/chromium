(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureSourceOverrideEnabled handles metadata');

  // `available` set to false causes PressureObserver.observe() to fail.
  await dp.Emulation.setPressureSourceOverrideEnabled({
    enabled: true,
    source: 'cpu',
    metadata: {available: false},
  });
  const errorName = await session.evaluateAsync(`
    new Promise((resolve, reject) => {
      const observer = new PressureObserver(reject);
      observer.observe("cpu").catch(e => {
        resolve(e.name);
      });
    })
  `);
  testRunner.log(`PressureObserver.observe() failed with ${errorName}`);

  testRunner.completeTest();
});

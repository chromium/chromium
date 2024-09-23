(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests removing a non-existent virtual pressure source is a no-op');

  testRunner.expectedSuccess(
      'Removing virtual pressure source does not fail',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        source: 'cpu',
        enabled: false,
      }));

  testRunner.completeTest();
});

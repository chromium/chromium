(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests removing an overridden virtual pressure source works');

  testRunner.expectedSuccess(
      'Creating pressure source override',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        source: 'cpu',
        enabled: true,
      }));
  testRunner.expectedSuccess(
      'Removing virtual pressure source does not fail',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        source: 'cpu',
        enabled: false,
      }));

  testRunner.completeTest();
});

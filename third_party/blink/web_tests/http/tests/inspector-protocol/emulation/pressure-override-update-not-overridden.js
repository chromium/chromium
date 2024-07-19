(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureStateOverride fails if pressure source is not overridden');

  testRunner.expectedError(
      'Setting state override fails',
      await dp.Emulation.setPressureStateOverride({
        source: 'cpu',
        state: 'critical',
      }));

  testRunner.completeTest();
});

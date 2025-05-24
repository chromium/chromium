(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureStateOverride fails if pressure source is not overridden');

  testRunner.expectedError(
      'Setting state override fails',
      await dp.Emulation.setPressureDataOverride({
        source: 'cpu',
        state: 'critical',
        ownContributionEstimate: 0.2
      }));

  testRunner.completeTest();
});

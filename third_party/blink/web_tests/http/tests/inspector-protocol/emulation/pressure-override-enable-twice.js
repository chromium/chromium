(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureSourceOverrideEnabled fails if enabling the same pressure source twice');

  testRunner.expectedSuccess(
      'Enabling pressure source override',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        enabled: true,
        source: 'cpu',
      }));

  // Test that the call fails regardless of changes to the `metadata` field.
  testRunner.log('Attempting to enable an already enabled pressure source');
  testRunner.log(await dp.Emulation.setPressureSourceOverrideEnabled({
    enabled: true,
    source: 'cpu',
  }));
  testRunner.log(await dp.Emulation.setPressureSourceOverrideEnabled({
    enabled: true,
    source: 'cpu',
    metadata: {available: false},
  }));

  testRunner.expectedSuccess(
      'Removing pressure source override',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        enabled: false,
        source: 'cpu',
      }));
  testRunner.expectedSuccess(
      'Enabling pressure source override again',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        enabled: true,
        source: 'cpu',
        metadata: {available: false},
      }));

  testRunner.completeTest();
});

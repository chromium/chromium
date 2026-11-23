(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that disabling page focus emulation restores document.hasFocus().');

  async function logFocus(label) {
    testRunner.log(`hasFocus(${label}): ` +
        await session.evaluate('document.hasFocus()'));
  }

  await logFocus('before emulation');

  await dp.Emulation.setFocusEmulationEnabled({enabled: true});
  await logFocus('after enable');

  await dp.Emulation.setFocusEmulationEnabled({enabled: false});
  await logFocus('after disable');

  testRunner.completeTest();
});

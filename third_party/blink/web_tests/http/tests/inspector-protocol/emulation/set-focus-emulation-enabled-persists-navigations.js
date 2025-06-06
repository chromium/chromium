(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session } = await testRunner
    .startBlank('Tests that page focus emulation persists between navigations');

  async function logFocus(session, label) {
    testRunner.log(`hasFocus(${label}):` + await session.evaluate('document.hasFocus()'));
  }

  await session.protocol.Emulation.setFocusEmulationEnabled({enabled: true});

  await logFocus(session, 'before navigation');

  await session.navigate('/resources/blank.html');

  await logFocus(session, 'after navigation');

  testRunner.completeTest();
})

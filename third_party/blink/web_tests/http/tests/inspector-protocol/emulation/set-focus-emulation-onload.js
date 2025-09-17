(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session } = await testRunner
    .startBlank('Tests that page focus emulation is correct while loading a document');

  async function logFocus(session, label) {
    testRunner.log(`hasFocus(${label}):` + await session.evaluate('document.hasFocus()'));
  }

  await logFocus(session, 'before emulation');

  await session.protocol.Emulation.setFocusEmulationEnabled({enabled: true});

  await logFocus(session, 'before navigation');

  await session.navigate(testRunner.url('resources/focus-on-load.html'));

  await logFocus(session, 'after navigation');

  testRunner.log(`had focus while loading:` + await session.evaluate('hadFocus'));

  testRunner.completeTest();
});

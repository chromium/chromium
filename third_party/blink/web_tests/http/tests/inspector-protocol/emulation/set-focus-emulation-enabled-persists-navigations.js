(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session } = await testRunner
    .startBlank('Tests that page focus emulation persists between navigations');

  async function logFocus(session, label) {
    testRunner.log(`hasFocus(${label}):` + await session.evaluate('document.hasFocus()'));
  }

  await logFocus(session, 'before emulation');

  await session.protocol.Emulation.setFocusEmulationEnabled({enabled: true});

  await logFocus(session, 'before navigation from about:blank');

  await session.navigate('/resources/blank.html');

  await logFocus(session, 'after navigation from about:blank');

  await session.navigate('http://devtools.oopif.test:8000/resources/blank.html');

  await logFocus(session, 'after cross-origin navigation');

  await session.navigate('http://devtools.oopif.test:8000/resources/blank.html');

  await logFocus(session, 'after same origin navigation');

  await session.protocol.Page.reload();

  await logFocus(session, 'after reload');

  testRunner.completeTest();
});

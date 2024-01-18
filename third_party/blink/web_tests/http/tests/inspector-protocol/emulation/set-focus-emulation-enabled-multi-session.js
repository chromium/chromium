(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank('Tests that page focus emulation is not reset by unrelated sessions');

  const page = await testRunner.createPage();

  const session1 = await page.createSession();
  const session2 = await page.createSession();

  async function logFocus(session, label) {
    testRunner.log(`hasFocus(${label}):` + await session.evaluate('document.hasFocus()'));
  }

  await logFocus(session1, 'session1, before emulation');
  await logFocus(session2, 'session2, before emulation');

  await session1.protocol.Emulation.setFocusEmulationEnabled({enabled: true});

  await logFocus(session1, 'session1, after emulation');
  await logFocus(session2, 'session2, after emulation');

  await session2.disconnect();

  await logFocus(session1, 'session1, after emulation, after session2 disconnect');

  await session1.disconnect();

  const session3 = await page.createSession();
  await logFocus(session3, 'session3, after emulation, after session1 disconnect');

  testRunner.completeTest();
})

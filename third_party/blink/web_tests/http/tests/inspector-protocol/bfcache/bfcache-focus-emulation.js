(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    'http://localhost:8000/inspector-protocol/resources/test-page.html',
    'Tests that document is updated after a BFCache navigation');
  await dp.Page.enable();

  async function logFocus(session, label) {
    testRunner.log(`hasFocus(${label}):` + await session.evaluate('document.hasFocus()'));
  }

  await logFocus(session, 'before emulation');

  await session.protocol.Emulation.setFocusEmulationEnabled({enabled: true});

  await logFocus(session, 'after emulation');

  // Regular navigation.
  session.navigate('https://devtools.oopif.test:8443/inspector-protocol/resources/iframe.html');
  await dp.Page.onceFrameNavigated();
  await logFocus(session, 'after regular navigation');

  // Navigate back - should use back-forward cache.
  const {result: history} = await dp.Page.getNavigationHistory();
  const entry = history.entries[history.currentIndex - 1];
  dp.Page.navigateToHistoryEntry({
    entryId: entry.id,
  });
  const frameNavigated = await dp.Page.onceFrameNavigated();
  testRunner.log(frameNavigated.params.type);
  await logFocus(session, 'after bfcache navigation');

  await session.protocol.Page.reload({
    ignoreCache: true
  });
  await dp.Page.onceFrameNavigated();
  await logFocus(session, 'after reload');

  testRunner.completeTest();
})

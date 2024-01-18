(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that Page.GetNavigationHistory(), ' +
          'Page.NavigateToHistoryEntry(), and Page.ResetNavigationHistory() ' +
          'in a fenced frame are not allowed.');
  await dp.Page.enable();
  await dp.Runtime.enable();

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  let {sessionId} = (await dp.Target.onceAttachedToTarget()).params;

  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  await ffdp.page.navigate(
      'http://localhost:8000/inspector-protocol/bfcache/resources/page-with-embed.html');

  const result =
      await childSession.sendCommand('Page.getNavigationHistory', {});
  testRunner.log(
      'Page.getNavigationHistory() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  await ffdp.Page.navigateToHistoryEntry({});
  testRunner.log(
      'Page.navigateToHistoryEntry() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  await ffdp.Page.ResetNavigationHistory({});
  testRunner.log(
      'Page.ResetNavigationHistory() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  testRunner.completeTest();
});

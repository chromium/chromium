(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that fenced frame target does not support Page.close()');
  await dp.Page.enable();
  await dp.Runtime.enable();

  await session.evaluate(() => {
    window.addEventListener('beforeunload', function(event) {
      console.log('beforeunload');
    }, false);
  });

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  let {sessionId} = (await dp.Target.onceAttachedToTarget()).params;

  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  // Fenced frames don't support Page.close().
  const result = await ffdp.Page.close();
  testRunner.log(
      'Page.close() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  dp.Page.close();
  // Console message should be emitted from inside the beforeunload handler.
  await dp.Runtime.onceConsoleAPICalled();
  testRunner.completeTest();
})

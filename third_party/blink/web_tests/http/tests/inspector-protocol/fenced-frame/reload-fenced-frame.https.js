(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that Page.reload() in a fenced frame is not allowed.');
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

  // reload() in a fenced frame gets an error.
  const result = await ffdp.Page.reload();
  testRunner.log(
      'Page.reload() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));
  testRunner.completeTest();
})

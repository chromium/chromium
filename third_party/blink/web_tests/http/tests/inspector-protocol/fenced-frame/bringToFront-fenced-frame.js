(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that Page.bringToFront() only works in a top level page.');
  await dp.Page.enable();
  await dp.Runtime.enable();

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  let {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  ffdp.Page.enable();
  ffdp.Runtime.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  // Fenced frames don't support Page.bringToFront().
  let result = await ffdp.Page.bringToFront();
  testRunner.log(
      'Page.bringToFront() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  result = await dp.Page.bringToFront();
  testRunner.log(
      'Page.bringToFront() from a top level page:\n' +
      (result.error ? 'FAIL: ' + result.error.message : 'PASS: no error'));

  testRunner.completeTest();
})

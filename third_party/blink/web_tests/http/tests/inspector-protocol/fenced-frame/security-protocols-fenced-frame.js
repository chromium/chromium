(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that Security.enable() doesn\'t work with a fenced frame page.');
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

  let result = await ffdp.Security.enable();
  testRunner.log(
    'Security.enable() from a fenced frame:\n' +
    (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  result = await dp.Page.bringToFront();
  testRunner.log(
      'Security.enable() from a top level frame:\n' +
      (result.error ? 'FAIL: ' + result.error.message : 'PASS: no error'));

  testRunner.completeTest()
})

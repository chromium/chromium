(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests that Page.createIsolatedWorld works when issued on paused target.');

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.navigate(testRunner.url('../resources/site_per_process_main.html'));
  const attached = await dp.Target.onceAttachedToTarget();
  const dp2 = session.createChild(attached.params.sessionId).protocol;
  if (!attached.params.waitingForDebugger)
    testRunner.log("FAIL: not waiting for debugger");
  const frameId = attached.params.targetInfo.targetId;
  dp2.Page.enable();
  const responsePromise = dp2.Page.createIsolatedWorld({frameId, worldName: 'Test world'});
  await dp2.Runtime.runIfWaitingForDebugger();
  testRunner.log((await responsePromise).result);
  testRunner.completeTest();
})

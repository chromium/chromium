(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('resources/page-with-nested-fenced-frame.html',
    'Tests that auto-attach with nested fenced frames works correctly');
  await dp.Page.enable();

  async function autoAttachToTarget(session) {
    const dp = session.protocol;
    dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
    return (await dp.Target.onceAttachedToTarget()).params;
  }

  let {sessionId} = await autoAttachToTarget(session);
  testRunner.log('attached to outer fenced frame');

  let childSession = session.createChild(sessionId);
  await autoAttachToTarget(childSession);
  testRunner.log('attached to inner fenced frame');

  testRunner.completeTest();
});

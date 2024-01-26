(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('resources/page-with-oopif-with-fenced-frame.html',
    'Tests that auto-attach of fenced frame inside oopif works correctly');
  await dp.Page.enable();

  async function autoAttachToTarget(session) {
    const dp = session.protocol;
    dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
    return (await dp.Target.onceAttachedToTarget()).params;
  }

  let { sessionId } = await autoAttachToTarget(session);
  testRunner.log('attached to iframe');

  let childSession = session.createChild(sessionId);
  await autoAttachToTarget(childSession);
  testRunner.log('attached to fenced frame');

  testRunner.completeTest();
});

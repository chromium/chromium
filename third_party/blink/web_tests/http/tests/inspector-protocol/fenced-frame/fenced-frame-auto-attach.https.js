(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('resources/page-with-fenced-frame.php',
    'Tests that target for fenced frame is auto attached and target info fields are correct');
  await dp.Page.enable();

  let attachedToTargetPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  let {targetInfo, sessionId} = (await attachedToTargetPromise).params;

  // Wait for fenced frame to finish loading - we need this because the
  // embedding document's load event is currently not blocked on the fenced
  // frame loading.
  let childSession = session.createChild(sessionId);
  let childDp = childSession.protocol;
  childDp.Page.enable();
  childDp.Page.setLifecycleEventsEnabled({ enabled: true });
  await childDp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  targetInfo = (await dp.Target.getTargetInfo({ targetId: targetInfo.targetId })).result.targetInfo;

  testRunner.log('Fenced frame target info: ')
  testRunner.log('type: ' + targetInfo.type);
  testRunner.log('url: ' + targetInfo.url);
  testRunner.log('title: ' + targetInfo.title);
  testRunner.log('openerId: ' + targetInfo.openerId);
  testRunner.log('openerFrameId: ' + targetInfo.openerFrameId);

  testRunner.completeTest();
});

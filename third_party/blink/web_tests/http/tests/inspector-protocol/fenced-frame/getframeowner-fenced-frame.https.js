(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('resources/page-with-fenced-frame.php',
    'Tests that DOM.getFrameOwner works correctly with Fenced Frame target');
  await dp.Page.enable();

  let attachedToTargetPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
  let { sessionId } = (await attachedToTargetPromise).params;
  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({ enabled: true });
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  const frameId = (await ffdp.Page.getFrameTree()).result.frameTree.frame.id;
  const backendNodeId = (await dp.DOM.getFrameOwner({frameId})).result.backendNodeId;
  const node = (await dp.DOM.describeNode({backendNodeId})).result.node;
  testRunner.log(`frame owner element: ${node.nodeName}`);

  testRunner.completeTest();
});

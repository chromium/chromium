(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('resources/page-with-fenced-frame.php',
    'Tests that permission policy for fenced frame can be retrieved correctly');
  await dp.Page.enable();

  dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
  let { sessionId } = (await dp.Target.onceAttachedToTarget()).params;
  let ffdp = session.createChild(sessionId).protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({ enabled: true });
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  const frameId = (await ffdp.Page.getFrameTree()).result.frameTree.frame.id;
  const states = (await ffdp.Page.getPermissionsPolicyState({frameId})).result.states
  let geolocationState = states.find(state => state.feature === 'geolocation');
  testRunner.log(geolocationState);

  testRunner.completeTest();
});

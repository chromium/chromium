(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that Page.disable() works with a fenced frame page.');
  await dp.Page.enable();
  await dp.Runtime.enable();

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  let {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  await ffdp.Runtime.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  ffdp.Page.onFrameNavigated((event) => {
    testRunner.log(`Frame navigated to ${event.params.frame.url}`);
  });

  await ffdp.Page.disable();
  // Try to navigate a page in a fenced frame after disabling. The navigation
  // should not be initiated even though the primary page is enabled still.
  await ffdp.Page.navigate(
      {url: testRunner.url('../fenced-frame/resources/page-with-title.php')});

  await ffdp.Page.enable();
  // Navigate a page in a fenced frame after enabling.
  await ffdp.Page.navigate(
      {url: testRunner.url('../fenced-frame/resources/page-with-title.php')});

  await ffdp.Page.onceFrameNavigated();
  testRunner.completeTest()
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that Page.stopLoading() in a fenced frame returns an error.');
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

  await ffdp.Network.setRequestInterception({patterns: [{}]});
  const navigatePromiseForFF = ffdp.Page.navigate(
      {url: testRunner.url('../resources/inspector-protocol-page.html')});
  const result = await ffdp.Page.stopLoading();
  testRunner.log(
      'Page.stopLoading() from a fenced frame:\n' +
      (result.error ? 'PASS: ' + result.error.message : 'FAIL: no error'));

  // Page.stopLoading() from a top level page cancels all navigation in the
  // page.
  dp.Page.stopLoading();
  await navigatePromiseForFF;
  testRunner.log('navigation finished');
  testRunner.completeTest();
})

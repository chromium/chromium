(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/page-with-fenced-frame.php',
      'Tests that global commands such as captureSnapshot, captureScreenshot, getAppManifest, startScreencast, or setDownloadBehavior ' +
          'returns an error with Fenced Frame target');
  await dp.Page.enable();

  dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
  let { sessionId } = (await dp.Target.onceAttachedToTarget()).params;

  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({ enabled: true });
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  async function checkCommand(method, params) {
    const result = await childSession.sendCommand(method, params);

    testRunner.log(`${method}: ` + (result.error ? result.error.message : 'FAIL: no error'));
  }

  await checkCommand('Page.captureSnapshot', {});
  await checkCommand('Page.captureScreenshot', {});
  await checkCommand('Page.getAppManifest', {});
  await checkCommand('Page.startScreencast', {});
  await checkCommand('Page.setDownloadBehavior', {behavior: 'deny'});

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {session, dp} = await testRunner.startBlank('Tests for certain commands are only available at the top-level target');

    await dp.Page.enable();
    await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
    dp.Page.navigate({url: testRunner.url('./resources/site_per_process_main.html')});

    const {params} = await dp.Target.onceAttachedToTarget();
    const childSession = session.createChild(params.sessionId);
    const dp2 = childSession.protocol;
    dp2.Page.enable();

    async function checkCommand(method, params) {
      const result = await childSession.sendCommand(method, params);

      testRunner.log(`${method}: ` + (result.error ? result.error.message : 'FAIL: no error'));
    }

    await checkCommand('Page.captureSnapshot', {});
    await checkCommand('Page.captureScreenshot', {});
    await checkCommand('Page.startScreencast', {});
    await checkCommand('Page.setDownloadBehavior', { behavior: 'deny'});

    testRunner.completeTest();
  })

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
    `Tests that Page.addScriptToEvaluateOnNewDocument on auto-attach with runImmediately=true.
Regression test for crbug.com/390710982.`);

  await dp.Page.enable();
  await dp.Target.enable();
  await dp.Target.setAutoAttach({ flatten: true, autoAttach: true, waitForDebuggerOnStart: true });

  dp.Target.onAttachedToTarget(async event => {
    const dp2 = session.createChild(event.params.sessionId).protocol;
    dp2.Page.enable();
    dp2.Runtime.enable();
    dp2.Runtime.onConsoleAPICalled(event => {
      testRunner.log(event, 'console called: ');
    });
    dp2.Page.addScriptToEvaluateOnNewDocument({
      source: 'console.log("evaluated")',
      runImmediately: true,
    });
    await dp2.Runtime.runIfWaitingForDebugger();
  });

  const loaded = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({
    url: testRunner.url('resources/iframe-src.html')
  });
  await loaded;

  testRunner.completeTest();
});

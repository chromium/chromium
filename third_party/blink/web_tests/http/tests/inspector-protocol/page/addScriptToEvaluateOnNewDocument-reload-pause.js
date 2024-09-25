(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Page.addScriptToEvaluateOnNewDocument can handle detached sessions (Regression test for crbug.com/368672129)`);

  await Promise.all([
    dp.Page.enable(),
    dp.Page.addScriptToEvaluateOnNewDocument({source: 'debugger'}),
    dp.Page.addScriptToEvaluateOnNewDocument({source: ''}), // The page handler might attempt to evaluate this on a disposed V8 session
    dp.Debugger.enable(),
    dp.Page.reload(),
  ]);

  await dp.Debugger.oncePaused();
  await session.disconnect();

  testRunner.completeTest();
});

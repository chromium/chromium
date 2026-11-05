(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies where the console message is logged for a module worker's import blocked by the page's policy.\n`);

  const logEntry = (target, entry) => {
    if (entry.source !== 'security') {
      return;
    }
    testRunner.log(`log target: ${target}`);
    testRunner.log(`entry.level: ${entry.level}`);
    testRunner.log(`entry.text: ${entry.text}`);
    testRunner.completeTest();
  };

  await dp.Log.enable();
  dp.Log.onEntryAdded(event => logEntry('creator', event.params.entry));

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  dp.Target.onAttachedToTarget(async event => {
    const wp = session.createChild(event.params.sessionId).protocol;
    await wp.Log.enable();
    wp.Log.onEntryAdded(event => logEntry('worker', event.params.entry));
    await wp.Runtime.runIfWaitingForDebugger();
  });

  page.navigate(
      'https://devtools.test:8443/inspector-protocol/resources/content-security-policy-worker-import.php');
})

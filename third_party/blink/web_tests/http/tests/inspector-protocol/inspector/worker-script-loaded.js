(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests Inspector.workerScriptLoaded.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const testWorker = async (url, type) => {
    testRunner.log(`creating worker '${url}'`);
    const workerAttached = dp.Target.onceAttachedToTarget();
    session.evaluateAsync(`window.worker = new Worker('${url}', { type: '${type}' });`);

    const workerSession = session.createChild((await workerAttached).params.sessionId);
    const wdp = workerSession.protocol;

    wdp.Runtime.enable();
    wdp.Runtime.onExceptionThrown(event => testRunner.log(event));
    wdp.Runtime.onConsoleAPICalled(event => testRunner.log(`message: ${event.params.args[0].value}`));
    wdp.Runtime.runIfWaitingForDebugger();
    await wdp.Inspector.onceWorkerScriptLoaded();
    testRunner.log('workerScriptLoaded');
  };

  await testWorker('/inspector-protocol/resources/workerClassic.js', 'classic');

  await testWorker('/inspector-protocol/resources/workerModule.js', 'module');

  testRunner.completeTest();
})

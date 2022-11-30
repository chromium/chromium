(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Fetch.requestPaused is emitted for the main script of dedciated worker`);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await dp.Fetch.enable({patterns: [{}]});
  dp.Fetch.onRequestPaused(event => {
    if (event.params.request.url.endsWith('/worker.js')) {
      dp.Fetch.fulfillRequest({
        requestId: event.params.requestId,
        responseCode: 200,
        responseHeaders: [{name: 'Content-Type', value: 'application/x-javascript'}],
        body: btoa(`console.log('PASSED: intercepted script')`)
      })
    }
  });
  const consoleMessagePromise = new Promise(resolve => {
    dp.Target.onAttachedToTarget(async event => {
      const wdp = session.createChild(event.params.sessionId).protocol;
      wdp.Runtime.enable();
      wdp.Runtime.onConsoleAPICalled(e => {
        resolve(e.params.args[0].value);
      })
      wdp.Runtime.runIfWaitingForDebugger();
    });
  });

  session.evaluate(`new Worker('/inspector-protocol/network/resources/worker.js')`);
  testRunner.log(await consoleMessagePromise);
  testRunner.completeTest();
})

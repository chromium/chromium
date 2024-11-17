(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Fetch.requestPaused is emitted for worker subresource when enable after worker creation`);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let workerResolve;
  const workerPromise = new Promise(resolve => workerResolve = resolve);
  let consoleResolve;
  const consolePromise = new Promise(resolve => consoleResolve = resolve);

  dp.Target.onAttachedToTarget(async event => {
    const wdp = session.createChild(event.params.sessionId).protocol;
    wdp.Runtime.enable();
    wdp.Runtime.onConsoleAPICalled(e => {
      consoleResolve(e.params.args[0].value);
    })
    wdp.Runtime.runIfWaitingForDebugger();
    workerResolve(wdp);
  });

  const workerScript = `
    onmessage = function(e) {
      fetch("http://127.0.0.1:8000/inspector-protocol/fetch/resources/fetch-data.txt").then(response => response.text()).then(console.log);
    };
  `;
  session.evaluate(`
    window.w = new Worker(URL.createObjectURL(new Blob(
      [${JSON.stringify(workerScript)}], { type: 'application/javascript' }
    )));
  `);
  await workerPromise;

  await dp.Fetch.enable({patterns: [{}]});
  dp.Fetch.onRequestPaused(event => {
    if (event.params.request.url.endsWith('/fetch-data.txt')) {
      dp.Fetch.fulfillRequest({
        requestId: event.params.requestId,
        responseCode: 200,
        responseHeaders: [{name: 'Content-Type', value: 'text/plain'}],
        body: btoa(`intercepted fetch-data`),
      });
    }
  });

  session.evaluate(`window.w.postMessage('')`);

  testRunner.log('fetched in worker: ' + await consolePromise);
  testRunner.completeTest();
})

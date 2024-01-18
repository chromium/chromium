(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that Network.requestWillBeSent and Fetch.requestPaused are emitted for worker importScripts before continuing request`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const importScriptRequestWillBeSent = new Promise(resolve => {
    dp.Target.onAttachedToTarget(async event => {
      const wdp = session.createChild(event.params.sessionId).protocol;
      wdp.Network.onRequestWillBeSent(e => {
        if (e.params.request.url.endsWith('/final.js')) {
          resolve(`Network.requestWillBeSent: ${e.params.request.url}`);
        }
      });
      await wdp.Network.enable();
      wdp.Runtime.runIfWaitingForDebugger();
    });
  });

  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{urlPattern: '*'}]});

  const importScriptRequestPaused = new Promise(resolve => {
    dp.Fetch.onRequestPaused(event => {
      if (event.params.request.url.endsWith('/final.js')) {
        // Do not continue this request! We want to see the
        // Network.requestWillBeSent without needing to continue the request.
        resolve(`Fetch.requestPaused: ${event.params.request.url}`);
      } else {
        dp.Fetch.continueRequest({
          requestId: event.params.requestId,
        });
      }
    });
  });

  await page.navigate(testRunner.url('./resources/worker-with-import.html'));
  testRunner.log(await importScriptRequestPaused);
  testRunner.log(await importScriptRequestWillBeSent);
  testRunner.completeTest();
})

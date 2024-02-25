(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that dedicated worker requests are intercepted.`);

  const FetchHelper = await testRunner.loadScript("resources/fetch-test.js");
  const globalFetcher = new FetchHelper(testRunner, testRunner.browserP());
  globalFetcher.setLogPrefix("[browser] ");
  await globalFetcher.enable();

  globalFetcher.onRequest().continueRequest({});

  await dp.Target.setAutoAttach({
      autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    const wdp = session.createChild(event.params.sessionId).protocol;
    await wdp.Runtime.runIfWaitingForDebugger();
  });

  await dp.Page.enable();
  await session.navigate("resources/empty.html");

  const result = await session.evaluateAsync(`
    const w = new Worker('/inspector-protocol/fetch/resources/worker.js');
    new Promise((resolve, reject) => {
      w.onmessage = e => resolve('worker is ready');
      w.onerror = e => reject(e.message);
      w.postMessage('start a worker');
    })
  `);

  testRunner.log(result);
  testRunner.completeTest();
});

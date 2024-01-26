(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that shared worker requests are intercepted.`);

  const FetchHelper = await testRunner.loadScript("resources/fetch-test.js");
  const globalFetcher = new FetchHelper(testRunner, testRunner.browserP());
  globalFetcher.setLogPrefix("[browser] ");
  await globalFetcher.enable();

  const workerBody = `
    self.testToken = 'OK: overriden worker body';
    self.addEventListener('connect', e => {
      e.ports[0].postMessage('ready');
    });
  `;

  globalFetcher.onceRequest().fulfill({
    responseCode: 200,
    responseHeaders: [{name: 'Content-Type', value: 'application/javascript'}],
    body: btoa(workerBody)
  });

  await session.evaluateAsync(`
    var test_worker = new SharedWorker('/inspector-protocol/fetch/resources/worker.js');
    test_worker.port.start();
    new Promise(fulfill => test_worker.port.addEventListener('message', e => {
      fulfill();
    }));
  `);

  const sharedWorker = (await dp.Target.getTargets()).result.targetInfos.filter(t => t.type == "shared_worker")[0];
  const {sessionId} = (await dp.Target.attachToTarget({targetId: sharedWorker.targetId, flatten: true})).result;
  const dp1 = session.createChild(sessionId).protocol;
  const workerFetcher = new FetchHelper(testRunner, dp1);
  workerFetcher.setLogPrefix("[worker] ");
  await workerFetcher.enable();
  workerFetcher.onRequest().continueRequest({});

  globalFetcher.onceRequest().fulfill({
    responseCode: 200,
    responseHeaders: [{name: 'Content-Type', value: 'application/javascript'}],
    body: btoa("overriden fetch body")
  });

  dp1.Runtime.enable();
  const testToken = (await dp1.Runtime.evaluate({
    expression: `self.testToken || 'n/a'`,
    returnByValue: true
  })).result.result.value;
  testRunner.log(`test token in overriden worker body: ${testToken}`);

  const body = (await dp1.Runtime.evaluate({
    expression: `fetch('/inspector-protocol/fetch/resources/hello-world.txt').then(r => r.text())`,
    awaitPromise: true,
    returnByValue: true
  })).result.result.value;

  testRunner.log(`got ${body}`);

  testRunner.completeTest();
})

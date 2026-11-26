(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startURL(
      'http://localhost:8000/inspector-protocol/resources/test-page.html',
      'Verify that preserved network conditions on a dedicated worker are ' +
          'cleared when the page is restored from the back/forward cache ' +
          'without a DevTools session reattaching to the worker.');

  await dp.Page.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'worker');

  await session.evaluateAsync(`
    new Promise(resolve => {
      window.worker = new Worker(
          '/inspector-protocol/network/resources/dedicated-worker-fetch.js');
      window.worker.onmessage = () => resolve();
      window.fetchInWorker = url => new Promise(res => {
        window.worker.onmessage = event => res(event.data);
        window.worker.postMessage(url);
      });
    })
  `);

  const attached = await attachedPromise;
  const workerSessionId = attached.params.sessionId;
  const workerDp = session.createChild(workerSessionId).protocol;

  await workerDp.Network.enable();
  const emulateResponse =
      await workerDp.Network.emulateNetworkConditionsByRule({
        matchedNetworkConditions: [{
          urlPattern: 'http://*:*/*hello-world.html*',
          offline: true,
          latency: 0,
          downloadThroughput: -1,
          uploadThroughput: -1,
        }],
      });
  testRunner.log(emulateResponse.error ||
                 'emulateNetworkConditionsByRule succeeded');

  const before = await session.evaluateAsync(
      `window.fetchInWorker('/inspector-protocol/network/resources/hello-world.html')`);
  testRunner.log(`Before navigation: ${before}`);

  const detached = dp.Target.onceDetachedFromTarget(
      event => event.params.sessionId === workerSessionId);
  await session.navigate(
      'http://devtools.oopif.test:8000/inspector-protocol/resources/empty.html');
  await detached;
  testRunner.log('Worker session detached');

  // Disable auto-attach so no session attaches to the worker on BFCache
  // restore. Preserved conditions must still be cleared on restore.
  await dp.Target.setAutoAttach(
      {autoAttach: false, waitForDebuggerOnStart: false, flatten: true});

  const frameNavigated = dp.Page.onceFrameNavigated();
  await dp.Runtime.evaluate({expression: 'window.history.back()'});
  const nav = await frameNavigated;
  testRunner.log(`Navigation type: ${nav.params.type}`);

  const after = await session.evaluateAsync(
      `window.fetchInWorker('/inspector-protocol/network/resources/hello-world.html')`);
  testRunner.log(`After BFCache restore: ${after}`);

  testRunner.completeTest();
});

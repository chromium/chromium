(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session} = await testRunner.startURL(
      'http://127.0.0.1:8000/inspector-protocol/network/resources/coi-page.php',
      'Verify that preserved network conditions on a dedicated worker are ' +
          'cleared when the root DevTools session of the client that set ' +
          'them is disposed.');

  const channelName = 'fetch-loop-worker-root-dispose';

  const observerPage = await testRunner.createPage();
  const observer = await observerPage.createSession();
  await observer.evaluate(`
    window.outcomes = {failed: 0, succeeded: 0};
    window.waiters = [];
    window.channel = new BroadcastChannel('${channelName}');
    window.channel.onmessage = event => {
      window.outcomes[event.data]++;
      const total = window.outcomes.failed + window.outcomes.succeeded;
      window.waiters = window.waiters.filter(waiter => {
        if (waiter.kind === 'total' && total < waiter.count) {
          return true;
        }
        if (waiter.kind === 'succeeded' &&
            window.outcomes.succeeded < waiter.count) {
          return true;
        }
        waiter.resolve({...window.outcomes});
        return false;
      });
    };
    window.outcomesAfter = count => new Promise(
        resolve => window.waiters.push({kind: 'total', count, resolve}));
    window.succeededAfter = count => new Promise(
        resolve => window.waiters.push({kind: 'succeeded', count, resolve}));
    undefined;
  `);

  // Attach a non-flattened session to the page so it owns a separate root
  // DevToolsSession that we can dispose independently of the test runner.
  const bp = testRunner.browserP();
  const rootSessionId = (await bp.Target.attachToTarget({
                          targetId: page.targetId(),
                          flatten: false,
                        })).result.sessionId;

  let nextId = 1;
  const pendingCalls = new Map();
  const eventWaiters = [];
  bp.Target.onReceivedMessageFromTarget(event => {
    if (event.params.sessionId !== rootSessionId) {
      return;
    }
    const msg = JSON.parse(event.params.message);
    if (msg.id && pendingCalls.has(msg.id)) {
      const resolve = pendingCalls.get(msg.id);
      pendingCalls.delete(msg.id);
      resolve(msg);
      return;
    }
    for (let i = eventWaiters.length - 1; i >= 0; --i) {
      if (eventWaiters[i].predicate(msg)) {
        const waiter = eventWaiters.splice(i, 1)[0];
        waiter.resolve(msg);
      }
    }
  });

  const sendToRoot = (method, params = {}, childSessionId = undefined) => {
    const id = nextId++;
    const payload = {id, method, params};
    if (childSessionId) {
      payload.sessionId = childSessionId;
    }
    const responsePromise =
        new Promise(resolve => pendingCalls.set(id, resolve));
    bp.Target.sendMessageToTarget(
        {sessionId: rootSessionId, message: JSON.stringify(payload)});
    return responsePromise;
  };

  const onceRootEvent = predicate =>
      new Promise(resolve => eventWaiters.push({predicate, resolve}));

  await sendToRoot('Target.setAutoAttach', {
    autoAttach: true,
    waitForDebuggerOnStart: true,
    flatten: true,
  });

  const workerAttachedPromise = onceRootEvent(
      msg => msg.method === 'Target.attachedToTarget' &&
          msg.params.targetInfo.type === 'worker');

  session.evaluate(`
    const sab = new SharedArrayBuffer(4);
    const releaseFlag = new Int32Array(sab);
    window.addEventListener('pagehide', () => {
      while (Atomics.load(releaseFlag, 0) === 0) {}
    });
    window.worker = new Worker(
        '/inspector-protocol/network/resources/fetch-loop-worker.php');
    window.worker.postMessage({sab, channelName: '${channelName}'});
    undefined;
  `);

  const workerAttached = await workerAttachedPromise;
  const workerSessionId = workerAttached.params.sessionId;

  await sendToRoot('Network.enable', {}, workerSessionId);
  const emulateResponse = await sendToRoot(
      'Network.emulateNetworkConditionsByRule', {
        matchedNetworkConditions: [{
          urlPattern: 'http://*:*/*hello-world.html*',
          offline: true,
          latency: 0,
          downloadThroughput: -1,
          uploadThroughput: -1,
        }],
      },
      workerSessionId);
  testRunner.log(emulateResponse.error ||
                 'emulateNetworkConditionsByRule succeeded');
  await sendToRoot('Runtime.runIfWaitingForDebugger', {}, workerSessionId);

  const before = await observer.evaluateAsync('window.outcomesAfter(20)');
  testRunner.log(
      `Matching requests failed before navigation: ${before.failed > 0}`);
  testRunner.log(
      `Matching requests succeeded before navigation: ${before.succeeded > 0}`);

  const workerDetachedPromise = onceRootEvent(
      msg => msg.method === 'Target.detachedFromTarget' &&
          msg.params.sessionId === workerSessionId);
  const navigated = session.navigate(
      'http://devtools.oopif.test:8000/inspector-protocol/resources/empty.html');
  await workerDetachedPromise;
  testRunner.log('Worker session detached');

  // Verify the preserved rule still blocks worker fetches while pagehide holds
  // the worker open.
  const atDetach = await observer.evaluate('({...window.outcomes})');
  const total = atDetach.failed + atDetach.succeeded;
  const afterDetach =
      await observer.evaluateAsync(`window.outcomesAfter(${total + 5})`);
  testRunner.log(`Matching requests failed after the detach: ${
      afterDetach.failed > atDetach.failed}`);
  testRunner.log(`Matching requests succeeded after the detach: ${
      afterDetach.succeeded > 0}`);

  // Dispose the root session; preserved conditions must be cleared immediately.
  await bp.Target.detachFromTarget({sessionId: rootSessionId});
  testRunner.log('Root session detached');

  const afterDispose = await observer.evaluateAsync('window.succeededAfter(1)');
  testRunner.log(`Matching request succeeded after root session disposed: ${
      afterDispose.succeeded > 0}`);

  await observer.evaluate(`window.channel.postMessage('release')`);
  await navigated;

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startURL(
      'http://127.0.0.1:8000/inspector-protocol/network/resources/coi-page.php',
      'Verify that Network.emulateNetworkConditionsByRule applies to the ' +
          'requests of a dedicated worker until the worker is terminated, ' +
          'when the page that owns the worker navigates.');

  const channelName = 'fetch-loop-worker-navigation';

  // A page of the same origin outlives the navigation and records the outcomes
  // that the worker reports.
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
        if (total < waiter.count) {
          return true;
        }
        waiter.resolve({...window.outcomes});
        return false;
      });
    };
    window.outcomesAfter = count => new Promise(
        resolve => window.waiters.push({count, resolve}));
    undefined;
  `);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  // The browser detaches the worker session before the old document unloads
  // or enters the back/forward cache. Block pagehide on an explicit signal
  // released via the worker so the test deterministically observes worker
  // requests after the detach.
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

  const attached = await dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'worker');
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
  await workerDp.Runtime.runIfWaitingForDebugger();

  const before = await observer.evaluateAsync('window.outcomesAfter(20)');
  testRunner.log(
      `Matching requests failed before navigation: ${before.failed > 0}`);
  testRunner.log(
      `Matching requests succeeded before navigation: ${before.succeeded > 0}`);

  const detached = dp.Target.onceDetachedFromTarget(
      event => event.params.sessionId === workerSessionId);
  const navigated = session.navigate(
      'http://devtools.oopif.test:8000/inspector-protocol/resources/empty.html');
  await detached;
  testRunner.log('Worker session detached');

  // The worker continues to send requests until pagehide is released.
  const atDetach = await observer.evaluate('({...window.outcomes})');
  const total = atDetach.failed + atDetach.succeeded;
  const after =
      await observer.evaluateAsync(`window.outcomesAfter(${total + 5})`);
  testRunner.log(`Matching requests failed after the detach: ${
      after.failed > atDetach.failed}`);
  testRunner.log(
      `Matching requests succeeded after the detach: ${after.succeeded > 0}`);

  await observer.evaluate(`window.channel.postMessage('release')`);
  await navigated;

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session} = await testRunner.startBlank(
      `Tests that Network.setExtraHTTPHeaders adds headers to WebSocket handshake requests created from a shared worker.`);

  const bp = testRunner.browserP();
  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = bp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'shared_worker');

  session.evaluate(`
    window.worker = new SharedWorker(
        '/inspector-protocol/network/websocket/resources/websocket-shared-worker.js');
  `);

  const {params: {sessionId}} = await attachedPromise;
  const workerdp = session.createChild(sessionId).protocol;

  await workerdp.Network.enable();
  await workerdp.Network.setExtraHTTPHeaders(
      {headers: {'X-DevTools-Test': 'Hello, world!'}});

  const handshakePromise =
      workerdp.Network.onceWebSocketWillSendHandshakeRequest();
  await workerdp.Runtime.runIfWaitingForDebugger();

  const {request} = (await handshakePromise).params;
  testRunner.log('X-DevTools-Test: ' + request.headers['X-DevTools-Test']);
  testRunner.completeTest();
})

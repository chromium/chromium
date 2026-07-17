(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Network.setExtraHTTPHeaders adds headers to WebSocket handshake requests created from a dedicated worker.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  // Headers set on the ancestor frame's target apply to the dedicated worker's
  // requests, just like they do for its subresource fetches.
  await dp.Network.enable();
  await dp.Network.setExtraHTTPHeaders({headers: {
    'X-DevTools-Frame': 'frame',
    'X-DevTools-Shared': 'from-frame',
  }});

  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'worker');
  session.evaluate(`
    window.worker = new Worker(
        '/inspector-protocol/network/websocket/resources/websocket-dedicated-worker.js');
  `);
  const {params: {sessionId}} = await attachedPromise;
  const workerdp = session.createChild(sessionId).protocol;

  // Headers set on the dedicated worker's own target (e.g. when inspected
  // directly from chrome://inspect/#workers) also apply, taking precedence over
  // the ancestor frame on conflicts.
  await workerdp.Network.enable();
  await workerdp.Network.setExtraHTTPHeaders({headers: {
    'X-DevTools-Worker': 'worker',
    'X-DevTools-Shared': 'from-worker',
  }});

  const handshakePromise =
      workerdp.Network.onceWebSocketWillSendHandshakeRequest();
  await workerdp.Runtime.runIfWaitingForDebugger();

  const {request} = (await handshakePromise).params;
  testRunner.log('X-DevTools-Frame: ' + request.headers['X-DevTools-Frame']);
  testRunner.log('X-DevTools-Worker: ' + request.headers['X-DevTools-Worker']);
  testRunner.log('X-DevTools-Shared: ' + request.headers['X-DevTools-Shared']);
  testRunner.completeTest();
})

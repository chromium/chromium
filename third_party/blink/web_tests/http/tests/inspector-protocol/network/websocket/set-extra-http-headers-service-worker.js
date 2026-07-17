(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      `Tests that Network.setExtraHTTPHeaders adds headers to WebSocket handshake requests created from a service worker.`);
  const swHelper = (await testRunner.loadScript(
      '../../service-worker/resources/service-worker-helper.js'))(dp, session);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const serviceWorkerURL =
      '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'service_worker');
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);
  const swdp = session.createChild((await attachedPromise).params.sessionId)
                   .protocol;

  await swdp.Network.enable();
  await swdp.Network.setExtraHTTPHeaders(
      {headers: {'X-DevTools-Test': 'Hello, world!'}});

  const handshakePromise =
      swdp.Network.onceWebSocketWillSendHandshakeRequest();
  swdp.Runtime.evaluate(
      {expression: `new WebSocket('ws://localhost:8880/echo');`});

  const {request} = (await handshakePromise).params;
  testRunner.log('X-DevTools-Test: ' + request.headers['X-DevTools-Test']);
  testRunner.completeTest();
})

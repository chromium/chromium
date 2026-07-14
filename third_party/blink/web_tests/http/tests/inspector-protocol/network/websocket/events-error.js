(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startBlank(
      `Tests that WebSocket frame error is emitted.`);

  await dp.Network.enable();

  dp.Network.onWebSocketCreated(event => {
    testRunner.log('WebSocketCreated');
    testRunner.log('  url: ' + event.params.url);
    testRunner.log('  has requestId: ' + !!event.params.requestId);
    testRunner.log('  has initiator: ' + !!event.params.initiator);
  });

  dp.Network.onWebSocketWillSendHandshakeRequest(event => {
    testRunner.log('WebSocketWillSendHandshakeRequest');
    testRunner.log('  has wallTime: ' + (event.params.wallTime > 0));
    testRunner.log('  has request headers: ' + !!event.params.request.headers);
  });

  dp.Network.onWebSocketFrameError(event => {
    testRunner.log('WebSocketFrameError');
    testRunner.log('  has timestamp: ' + (event.params.timestamp > 0));
    testRunner.log('  errorMessage: ' + event.params.errorMessage);
  });

  dp.Network.onWebSocketClosed(event => {
    testRunner.log('WebSocketClosed');
    testRunner.log('  has timestamp: ' + (event.params.timestamp > 0));
  });

  const evaluatePromise = session.evaluateAsync(`
    new Promise((resolve) => {
      var ws;
      function sendMessages() {
          ws = new WebSocket("ws://127.0.0.1:8000/does_not_exist");
          ws.onclose = function()
          {
              resolve();
          };
      }
      sendMessages();
    })
  `);
  await dp.Network.onceWebSocketClosed();
  await evaluatePromise;

  testRunner.completeTest();
})

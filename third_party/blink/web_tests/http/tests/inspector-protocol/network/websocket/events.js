(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that WebSocket related CDP events are emitted.`);

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

  dp.Network.onWebSocketHandshakeResponseReceived(event => {
    testRunner.log('WebSocketHandshakeResponseReceived');
    testRunner.log('  status: ' + event.params.response.status);
    testRunner.log('  has response headers: ' +
                   !!event.params.response.headers);
  });

  dp.Network.onWebSocketFrameSent(event => {
    testRunner.log('WebSocketFrameSent');
    testRunner.log('  payload: ' + event.params.response.payloadData);
    testRunner.log('  opcode: ' + event.params.response.opcode);
    testRunner.log('  mask: ' + event.params.response.mask);
  });

  dp.Network.onWebSocketFrameReceived(event => {
    testRunner.log('WebSocketFrameReceived');
    testRunner.log('  payload: ' + event.params.response.payloadData);
    testRunner.log('  opcode: ' + event.params.response.opcode);
    testRunner.log('  mask: ' + event.params.response.mask);
  });

  dp.Network.onWebSocketClosed(event => {
    testRunner.log('WebSocketClosed');
    testRunner.log('  has timestamp: ' + (event.params.timestamp > 0));
  });

  const evaluatePromise = session.evaluateAsync(`
    new Promise((resolve) => {
      var ws;
      function sendMessages() {
          ws = new WebSocket("ws://localhost:8880/echo");
          ws.onopen = function()
          {
              ws.send("test");
              ws.send("exit");
          };
          let messageCount = 0;
          ws.onmessage = function() {
            messageCount++;
            if (messageCount === 2) {
              ws.close();
              resolve();
            }
          };
      }
      sendMessages();
    })
  `);
  await dp.Network.onceWebSocketClosed();
  await evaluatePromise;

  testRunner.completeTest();
})

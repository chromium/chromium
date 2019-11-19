// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Verifies that HAR exports contain websocket messages');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.NetworkAgent.setCacheDisabled(true);

  const lastMessagePromise = new Promise(resolve => {
    TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestUpdated, event => {
      const request = event.data;
      if (!request.frames().length)
        return;
      const lastFrame = request.frames()[request.frames().length - 1];
      if (lastFrame.text === 'last message' && lastFrame.type === SDK.NetworkRequest.WebSocketFrameType.Receive)
        resolve();
    });
  });

  await TestRunner.evaluateInPagePromise(`
      new Promise(resolve => {
        ws = new WebSocket('ws://127.0.0.1:8880/echo');
        ws.onopen = () => {
          ws.send('text message');
          ws.send(new TextEncoder().encode('binary message'));
          ws.send('last message');
        };
      });
    `);
  await lastMessagePromise;


  const harString = await new Promise(async resolve => {
    const stream = new TestRunner.StringOutputStream(resolve);
    const progress = new Common.Progress();
    await Network.HARWriter.write(
        stream, NetworkTestRunner.networkRequests(), progress);
    progress.done();
    stream.close();
  });
  const har = JSON.parse(harString);

  const websocketEntry = har.log.entries.find(entry => entry.request.url.endsWith('/echo'));
  const messages = websocketEntry._webSocketMessages.map(message => {
    return {
      type: message.type,
      opcode: message.opcode,
      data: message.data
    };
  }).sort((messageOne, messageTwo) => {
    return (messageOne.type + messageOne.data).localeCompare(messageTwo.type + messageTwo.data);
  });
  TestRunner.addResult('messages: ' + JSON.stringify(messages, null, 2));
  TestRunner.completeTest();
})();

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that WebSocket does not send messages when emulating offline network.`);
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => testRunner.die('Timeout', errorForLog), 5000);

  await dp.Network.enable();
  errorForLog = new Error();

  await session.evaluateAsync(`
        log = '';
        new Promise((resolve) => {
          broadcaster_ws = new WebSocket('ws://localhost:8880/network_emulation?role=broadcaster');
          broadcaster_ws.onerror = () => log += 'onerror ';
          broadcaster_ws.onopen = () => {
            log += 'onopen ';
            resolve();
          };
        });`);
  errorForLog = new Error();
  await dp.Network.emulateNetworkConditions({
    offline: true,
    downloadThroughput: -1,
    uploadThroughput: -1,
    latency: 0,
  });
  errorForLog = new Error();
  const listener_ws = await new Promise((resolve) => {
    const ws =
        new WebSocket('ws://localhost:8880/network_emulation?role=listener');
    ws.onopen = () => resolve(ws);
    ws.onerror = () => testRunner.log('onerror: unexpected error in listener_ws');
    ws.onclose = () => testRunner.log('onclose: unexpected close of listener_ws');
  });
  errorForLog = new Error();

  const messageRecieved = new Promise((resolve) => {
    listener_ws.onmessage = async (msg) => {
      if (msg.data == 'Offline')
        testRunner.log('onmessage: unexpected message ');
      resolve();
    };
  });

  await session.evaluateAsync(`broadcaster_ws.send('Offline');`);
  errorForLog = new Error();
  listener_ws.send('Control');
  await messageRecieved;
  errorForLog = new Error();
  testRunner.log(await session.evaluateAsync(`log`));
  errorForLog = new Error();

  testRunner.completeTest();
})

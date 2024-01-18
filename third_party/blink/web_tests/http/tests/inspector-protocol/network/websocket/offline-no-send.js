// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that WebSocket does not send messages when emulating offline network.`);

  await dp.Network.enable();

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
  await dp.Network.emulateNetworkConditions({
    offline: true,
    downloadThroughput: -1,
    uploadThroughput: -1,
    latency: 0,
  });
  const listener_ws = await new Promise((resolve) => {
    const ws =
        new WebSocket('ws://localhost:8880/network_emulation?role=listener');
    ws.onopen = () => resolve(ws);
  });

  const messageRecieved = new Promise((resolve) => {
    listener_ws.onmessage = async (msg) => {
      if (msg.data == 'Offline')
        testRunner.log('onmessage: unexpected message ');
      resolve();
    };
  });

  await session.evaluateAsync(`broadcaster_ws.send('Offline');`);
  listener_ws.send('Control');
  await messageRecieved;
  testRunner.log(await session.evaluateAsync(`log`));

  testRunner.completeTest();
})

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that WebSocket does not recieve messages when emulating offline network.`);

  await dp.Network.enable();

  await session.evaluateAsync(`
        log = '';
        new Promise((resolve) => {
          listener_ws = new WebSocket('ws://localhost:8880/network_emulation?role=listener');
          listener_ws.onerror = () => log += 'onerror ';
          listener_ws.onopen = resolve;
          listener_ws.onmessage = (msg) => log += 'onmessage: unexpected message ';
        });`);
  await dp.Network.emulateNetworkConditions({
    offline: true,
    downloadThroughput: -1,
    uploadThroughput: -1,
    latency: 0,
  });

  // Here we are creating a websocket connection to the same server outside of
  // 'session' where network emulation does not apply. Sending a message via
  // broadcaster_ws here will cause the websocket server to send message to the
  // other client running in 'session' where we emulate offline.
  // If the emulation works correctly, this message should not be received.
  const broadcaster_ws =
      new WebSocket('ws://localhost:8880/network_emulation?role=broadcaster');
  const messageRecieved =
      new Promise((resolve) => {broadcaster_ws.onmessage = resolve});
  broadcaster_ws.onopen = () => broadcaster_ws.send('test');
  await messageRecieved;

  testRunner.log(await session.evaluateAsync(`log`));
  testRunner.completeTest();
})

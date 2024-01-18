// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that WebSocket appears slower when emulating slow network.`);

  await dp.Network.enable();

  await session.evaluateAsync(`
      function measureTime() {
        return new Promise((resolve) => {
          const ws = new WebSocket('ws://localhost:8880/echo');
          let startTime;
          let numMessages = 0;
          ws.onopen = () => {
            startTime = performance.now();
            ws.send('x'.repeat(1000));
            ws.send('x'.repeat(1000));
          };
          ws.onmessage = () => {
            ++numMessages;
            if (numMessages == 2) resolve(performance.now() - startTime);
          }
          ws.onerror = () => log += 'onerror ';
          ws.onclose = () => log += 'onclose ';
        });
      }`);
  const normalTime = await session.evaluateAsync(`measureTime()`);
  await dp.Network.emulateNetworkConditions({
    offline: false,
    downloadThroughput: 10000,
    uploadThroughput: 0,
    latency: 0,
  });
  const emulatedSlowDownloadTime = await session.evaluateAsync(`measureTime()`);

  testRunner.log(
      'websocket is slower when emulating slow download: ' +
      ((emulatedSlowDownloadTime - normalTime) > 10));

  await dp.Network.emulateNetworkConditions({
    offline: false,
    downloadThroughput: 0,
    uploadThroughput: 10000,
    latency: 0,
  });
  const emulatedSlowUploadTime = await session.evaluateAsync(`measureTime()`);

  testRunner.log(
      'websocket is slower when emulating slow upload: ' +
      ((emulatedSlowUploadTime - normalTime) > 10));
  await dp.Network.emulateNetworkConditions({
    offline: false,
    downloadThroughput: -1,
    uploadThroughput: -1,
    latency: 0,
  });

  testRunner.completeTest();
})

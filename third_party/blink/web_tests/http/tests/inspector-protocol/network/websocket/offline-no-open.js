// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that WebSocket does not open connections when emulating offline network.`);


  await dp.Network.enable();

  await dp.Network.emulateNetworkConditions({
    offline: true,
    downloadThroughput: -1,
    uploadThroughput: -1,
    latency: 0,
  });

  testRunner.log(await session.evaluateAsync(`
        new Promise((resolve) => {
          let log = '';
          const ws = new WebSocket('ws://localhost:8880/echo');
          ws.onopen = () => {log += 'onopen '; ws.close(); };
          ws.onmessage = () => log += 'onmessage ';
          ws.onerror = () => log += 'onerror ';
          ws.onclose = () => {
            log += 'onclose ';
            resolve(log);
          };
        });`));
  await dp.Network.emulateNetworkConditions({
    offline: false,
    downloadThroughput: -1,
    uploadThroughput: -1,
    latency: 0,
  });
  testRunner.completeTest();
})

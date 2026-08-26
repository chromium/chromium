// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests Browser.addMockCamera device-list updates and session cleanup.');

  async function waitForVideoInputCount(expectedCount) {
    return await session.evaluateAsync(async expectedCount => {
      for (let attempt = 0; attempt < 50; ++attempt) {
        const devices = await navigator.mediaDevices.enumerateDevices();
        const count =
            devices.filter(device => device.kind === 'videoinput').length;
        if (count === expectedCount)
          return true;
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      return false;
    }, expectedCount);
  }

  testRunner.log(
      `Initially no video inputs: ${await waitForVideoInputCount(0)}`);

  const browserSession = await testRunner.attachFullBrowserSession();
  await browserSession.protocol.Browser.addMockCamera({deviceId: 'camera-1'});
  testRunner.log(`Mock camera added: ${await waitForVideoInputCount(1)}`);

  await browserSession.disconnect();
  testRunner.log(`Mock camera removed after session disconnect: ${
      await waitForVideoInputCount(0)}`);

  testRunner.completeTest();
})

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests opening a Browser.addMockCamera camera by exact deviceId.');

  async function openMockCamera() {
    return await session.evaluateAsync(async () => {
      const withTimeout = (promise, message) => {
        let timeoutId;
        const timeoutPromise = new Promise((_, reject) => {
          timeoutId = setTimeout(() => reject(new Error(message)), 5000);
        });
        return Promise.race([promise, timeoutPromise])
            .finally(() => clearTimeout(timeoutId));
      };

      let videoInputs = [];
      for (let attempt = 0; attempt < 50; ++attempt) {
        const devices = await navigator.mediaDevices.enumerateDevices();
        videoInputs = devices.filter(device => device.kind === 'videoinput');
        if (videoInputs.length === 1)
          break;
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      if (videoInputs.length !== 1)
        return false;

      try {
        const warmupStream = await withTimeout(
            navigator.mediaDevices.getUserMedia({video: true}),
            'getUserMedia timed out');
        warmupStream.getTracks().forEach(track => track.stop());

        const devices = await navigator.mediaDevices.enumerateDevices();
        const camera = devices.find(device => device.kind === 'videoinput');
        if (!camera || !camera.deviceId)
          return false;

        const stream = await withTimeout(navigator.mediaDevices.getUserMedia({
          video: {deviceId: {exact: camera.deviceId}},
        }),
                                         'Exact-device getUserMedia timed out');
        const tracks = stream.getVideoTracks();
        const success = tracks.length === 1 && tracks[0].readyState === 'live';
        stream.getTracks().forEach(track => track.stop());
        return success;
      } catch {
        return false;
      }
    });
  }

  const browserSession = await testRunner.attachFullBrowserSession();
  await browserSession.protocol.Browser.addMockCamera({deviceId: 'camera-1'});
  testRunner.log(
      `Exact deviceId opened one live video track: ${await openMockCamera()}`);

  await browserSession.disconnect();
  testRunner.completeTest();
})

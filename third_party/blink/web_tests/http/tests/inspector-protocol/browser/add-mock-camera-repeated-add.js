// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests that duplicate mock-camera adds preserve a live stream.');
  let browserSession;
  let permissionSession;

  function checkResponse(response, command) {
    if (response.error)
      throw new Error(`${command}: ${JSON.stringify(response.error)}`);
  }

  function checkDuplicateError(response) {
    if (response.error?.message !==
        'A mock camera with this deviceId already exists') {
      throw new Error(
          `Expected a duplicate ID error: ${JSON.stringify(response)}`);
    }
  }

  async function waitForCameraIds(count) {
    const result = await session.evaluateAsync(async count => {
      let ids = [];
      for (let attempt = 0; attempt < 50; ++attempt) {
        ids = (await navigator.mediaDevices.enumerateDevices())
                  .filter(device => device.kind === 'videoinput')
                  .map(device => device.deviceId);
        if (ids.length === count && ids.every(id => id.length > 0))
          return {ids};
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      return {
        error: `Expected ${count} cameras; last IDs: ${JSON.stringify(ids)}`
      };
    }, count);
    if (result.error)
      throw new Error(result.error);
    return result.ids;
  }

  async function openCamera(deviceId) {
    const result = await session.evaluateAsync(async deviceId => {
      const state = window.mockCameraTest = {
        closed: false,
        ended: false,
        lastMediaTime: -1,
        stream: null,
        video: document.createElement('video'),
      };
      const withTimeout = (promise, message) => {
        let timeoutId;
        const timeout = new Promise((_, reject) => {
          timeoutId = setTimeout(() => reject(new Error(message)), 5000);
        });
        return Promise.race([promise, timeout])
            .finally(() => clearTimeout(timeoutId));
      };

      try {
        state.stream = await withTimeout(
            navigator.mediaDevices
                .getUserMedia({
                  video: {deviceId: {exact: deviceId}},
                })
                .then(stream => {
                  // A timed-out request may still resolve after test cleanup.
                  if (state.closed) {
                    stream.getTracks().forEach(track => track.stop());
                    throw new Error('Capture resolved after cleanup');
                  }
                  state.stream = stream;
                  return stream;
                }),
            'getUserMedia timed out');
        const tracks = state.stream.getVideoTracks();
        if (tracks.length !== 1 || tracks[0].readyState !== 'live')
          throw new Error('Expected one live video track');
        state.track = tracks[0];
        state.track.addEventListener('ended', () => state.ended = true);
        state.video.muted = true;
        state.video.playsInline = true;
        state.video.srcObject = state.stream;
        document.body.appendChild(state.video);

        state.waitForFrames = count => new Promise((resolve, reject) => {
          if (state.ended || state.track.readyState !== 'live') {
            reject(new Error('The original track has ended'));
            return;
          }
          let callbackId;
          let received = 0;
          const timeoutId = setTimeout(
              () => finish(new Error('Timed out waiting for new frames')),
              5000);
          const onEnded = () => finish(new Error('The original track ended'));
          const onError = () => finish(new Error('Video playback failed'));
          function finish(error) {
            clearTimeout(timeoutId);
            state.video.cancelVideoFrameCallback(callbackId);
            state.track.removeEventListener('ended', onEnded);
            state.video.removeEventListener('error', onError);
            state.cancelWait = null;
            error ? reject(error) : resolve();
          }
          function onFrame(now, metadata) {
            // Do not count a redisplay of the last frame as new camera output.
            if (metadata.mediaTime > state.lastMediaTime) {
              state.lastMediaTime = metadata.mediaTime;
              ++received;
            }
            if (received >= count) {
              finish();
              return;
            }
            callbackId = state.video.requestVideoFrameCallback(onFrame);
          }
          state.cancelWait = () => finish(new Error('Capture was cleaned up'));
          state.track.addEventListener('ended', onEnded);
          state.video.addEventListener('error', onError);
          callbackId = state.video.requestVideoFrameCallback(onFrame);
        });

        await withTimeout(state.video.play(), 'video.play() timed out');
        await state.waitForFrames(2);
        return {};
      } catch (error) {
        state.closed = true;
        state.stream?.getTracks().forEach(track => track.stop());
        return {error: `${error.name}: ${error.message}`};
      }
    }, deviceId);
    if (result.error)
      throw new Error(result.error);
  }

  async function cleanup(action) {
    try {
      await action();
    } catch (error) {
      testRunner.log(`Cleanup failed: ${error.message}`);
    }
  }

  try {
    // Expose the page-visible ID without a warm-up capture or a second stream.
    permissionSession = await testRunner.attachFullBrowserSession();
    checkResponse(await permissionSession.protocol.Browser.setPermission({
      permission: {name: 'camera'},
      setting: 'granted',
      origin: await session.evaluate('location.origin'),
    }),
                  'Browser.setPermission');
    await waitForCameraIds(0);

    browserSession = await testRunner.attachFullBrowserSession();
    const responses = await Promise.all([
      browserSession.protocol.Browser.addMockCamera({deviceId: 'camera-1'}),
      browserSession.protocol.Browser.addMockCamera({deviceId: 'camera-1'}),
    ]);
    checkResponse(responses[0], 'Initial addMockCamera');
    checkDuplicateError(responses[1]);
    testRunner.log('The concurrent duplicate add is rejected: true');
    const [deviceId] = await waitForCameraIds(1);
    testRunner.log('A mock camera is registered: true');
    await openCamera(deviceId);
    testRunner.log('The camera delivers frames before duplicate add: true');

    checkDuplicateError(await browserSession.protocol.Browser.addMockCamera(
        {deviceId: 'camera-1'}));
    testRunner.log('The duplicate add with a live stream is rejected: true');
    const [remainingId] = await waitForCameraIds(1);
    if (remainingId !== deviceId)
      throw new Error('Duplicate add changed the page-visible deviceId');
    testRunner.log('Duplicate add preserves the page-visible deviceId: true');

    const result = await session.evaluateAsync(async () => {
      const state = window.mockCameraTest;
      try {
        await state.waitForFrames(3);
        return {
          live: !state.ended && state.track.readyState === 'live' &&
              state.stream.getVideoTracks()[0] === state.track,
        };
      } catch (error) {
        return {error: `${error.name}: ${error.message}`};
      }
    });
    if (result.error)
      throw new Error(result.error);
    if (!result.live)
      throw new Error('The original track did not remain live');
    testRunner.log('The original track remains live: true');
    testRunner.log('The original stream continues to deliver frames: true');
  } catch (error) {
    testRunner.log(`FAIL: ${error.message}`);
  } finally {
    await cleanup(() => session.evaluate(() => {
      const state = window.mockCameraTest;
      if (!state)
        return;
      state.closed = true;
      state.cancelWait?.();
      state.stream?.getTracks().forEach(track => track.stop());
      state.video.srcObject = null;
      state.video.remove();
      delete window.mockCameraTest;
    }));
    if (browserSession) {
      await cleanup(() => browserSession.disconnect());
      await cleanup(async () => {
        await waitForCameraIds(0);
        testRunner.log('The camera is removed after session disconnect: true');
      });
    }
    if (permissionSession) {
      await cleanup(async () => {
        checkResponse(
            await permissionSession.protocol.Browser.resetPermissions(),
            'Browser.resetPermissions');
      });
      await cleanup(() => permissionSession.disconnect());
    }
    testRunner.completeTest();
  }
})

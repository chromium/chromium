// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests mock camera IDs and cleanup across CDP sessions.');
  const cameraSessions = new Set();
  let permissionSession;

  function checkResponse(response, command) {
    if (response.error)
      throw new Error(`${command}: ${JSON.stringify(response.error)}`);
  }

  async function attachCameraSession() {
    const browserSession = await testRunner.attachFullBrowserSession();
    cameraSessions.add(browserSession);
    return browserSession;
  }

  async function disconnectCameraSession(browserSession) {
    await browserSession.disconnect();
    cameraSessions.delete(browserSession);
  }

  async function waitForCameraIds(count, expectedIds = null) {
    const result = await session.evaluateAsync(async ({count, expectedIds}) => {
      let ids = [];
      for (let attempt = 0; attempt < 50; ++attempt) {
        const devices = await navigator.mediaDevices.enumerateDevices();
        ids = devices.filter(device => device.kind === 'videoinput')
                  .map(device => device.deviceId);
        if (ids.length === count && ids.every(id => id.length > 0) &&
            new Set(ids).size === count &&
            (!expectedIds || expectedIds.every(id => ids.includes(id)))) {
          return {ids};
        }
        await new Promise(resolve => setTimeout(resolve, 100));
      }
      return {
        error: `Timed out waiting for ${count} cameras with matching IDs; ` +
            `last IDs: ${JSON.stringify(ids)}`
      };
    }, {count, expectedIds});
    if (result.error)
      throw new Error(result.error);
    return result.ids;
  }

  async function checkCleanup(firstId, secondId, disconnectFirst) {
    await waitForCameraIds(0);
    const first = await attachCameraSession();
    const second = await attachCameraSession();

    checkResponse(
        await first.protocol.Browser.addMockCamera({deviceId: firstId}),
        'Session A addMockCamera');
    const [firstCameraId] = await waitForCameraIds(1);

    checkResponse(
        await second.protocol.Browser.addMockCamera({deviceId: secondId}),
        'Session B addMockCamera');
    const ids = await waitForCameraIds(2);
    if (!ids.includes(firstCameraId))
      throw new Error(
          'Adding session B\'s camera replaced session A\'s camera');
    const secondCameraId = ids.find(id => id !== firstCameraId);
    testRunner.log('Both sessions have distinct cameras: true');

    const disconnected = disconnectFirst ? first : second;
    const remaining = disconnectFirst ? second : first;
    const remainingId = disconnectFirst ? secondCameraId : firstCameraId;
    await disconnectCameraSession(disconnected);
    await waitForCameraIds(1, [remainingId]);
    testRunner.log('The other session\'s camera remains unchanged: true');

    await disconnectCameraSession(remaining);
    await waitForCameraIds(0);
    testRunner.log('Both cameras are removed after disconnect: true');
  }

  async function cleanup(action) {
    try {
      await action();
    } catch (error) {
      testRunner.log(`Cleanup failed: ${error.message}`);
    }
  }

  try {
    // Keep permission overrides independent of the sessions being disconnected.
    // Grant access to device IDs without opening a capture stream.
    permissionSession = await testRunner.attachFullBrowserSession();
    checkResponse(await permissionSession.protocol.Browser.setPermission({
      permission: {name: 'camera'},
      setting: 'granted',
      origin: await session.evaluate('location.origin'),
    }),
                  'Browser.setPermission');

    testRunner.log('Different logical IDs:');
    await checkCleanup('camera-1', 'camera-2', true);
    testRunner.log('Same logical ID, session A disconnects first:');
    await checkCleanup('camera-1', 'camera-1', true);
    testRunner.log('Same logical ID, session B disconnects first:');
    await checkCleanup('camera-1', 'camera-1', false);
  } catch (error) {
    testRunner.log(`FAIL: ${error.message}`);
  } finally {
    for (const browserSession of cameraSessions)
      await cleanup(() => disconnectCameraSession(browserSession));
    await cleanup(() => waitForCameraIds(0));
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

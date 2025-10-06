(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that permission can be changed and properly queried from iframes.`);
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await dp.Browser.resetPermissions();

  const embeddingUrl =
      'http://embedding.test:8000/inspector-protocol/network/resources/page-with-iframe.html';
  const requestingUrl =
      'http://devtools.oopif.test:8000/inspector-protocol/resources/empty.html';
  const storage_access_descriptor = {name: 'storage-access'};

  await page.navigate(embeddingUrl);
  setIframeSrc(requestingUrl);
  let iframeSession =
      getAttachedSession(await dp.Target.onceAttachedToTarget());

  // Grant and query the storage access permission.
  await queryPermission(iframeSession, storage_access_descriptor, 'prompt');
  await setPermission(
      storage_access_descriptor, 'granted', embeddingUrl, requestingUrl);
  await queryPermission(iframeSession, storage_access_descriptor, 'granted');

  // Navigate to a new embedding url.
  const newEmbeddingUrl =
      'http://new-embedding.test:8000/inspector-protocol/network/resources/page-with-iframe.html';

  await page.navigate(newEmbeddingUrl);
  setIframeSrc(requestingUrl);
  iframeSession = getAttachedSession(await dp.Target.onceAttachedToTarget());

  // Permission query should be "prompt" for a different embedding site.
  await queryPermission(iframeSession, storage_access_descriptor, 'prompt');

  // Setting a permission without a requesting origin should set the permission
  // globally.
  const notifications_descriptor = {name: 'notifications'};
  await queryPermission(iframeSession, notifications_descriptor, 'denied');
  await setPermission(notifications_descriptor, 'granted');
  await queryPermission(iframeSession, notifications_descriptor, 'granted');

  // Setting a permission with opaque origins should return the correct error
  // message.
  await setPermission(storage_access_descriptor, 'granted', 'data:text/html,');
  await setPermission(
      storage_access_descriptor, 'granted', 'data:text/html,', requestingUrl);

  testRunner.completeTest();

  async function setPermission(permission, setting, origin, embeddedOrigin) {
    const params = {permission, setting};
    if (origin) {
      params.origin = origin;
    }
    if (embeddedOrigin) {
      params.embeddedOrigin = embeddedOrigin;
    }
    const response = await dp.Browser.setPermission(params);
    if (response.error) {
      testRunner.log(`- Failed to set permission: ${
          JSON.stringify(permission)} error: ${response.error.message}`);
    } else {
      testRunner.log(`- Set: ${JSON.stringify(permission)} to '${setting}'`);
    }
  }

  async function queryPermission(targetSession, descriptor, expectedState) {
    const result = await targetSession.evaluateAsync(async (descriptor) => {
      const status = await navigator.permissions.query(descriptor);
      return status.state;
    }, descriptor);

    if (result === expectedState) {
      testRunner.log(`Query success: ${JSON.stringify(descriptor)}: ${result}`);
    } else {
      testRunner.log(
          `Query failed: For ${JSON.stringify(descriptor)}, expected ${
              expectedState}, but got ${result}`);
    }
  }

  function getAttachedSession(attachedEvent) {
    const newSession = session.createChild(attachedEvent.params.sessionId);
    return newSession;
  }

  function setIframeSrc(url) {
    return session.evaluate(`document.getElementById('iframe').src = '${url}'`);
  }
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that response body is available after the page consumed the fetch response as a blob.`);

  await dp.Network.enable();

  async function logResponseBody(url) {
    const blobSizePromise = session.evaluateAsync(`
        fetch(${JSON.stringify(url)}).then(r => r.blob()).then(b => b.size)`);
    const requestId =
        (await dp.Network.onceRequestWillBeSent()).params.requestId;
    // The body must be readable as soon as loading has finished, without
    // waiting for the page to observe the blob.
    await dp.Network.onceLoadingFinished(e => e.params.requestId === requestId);
    const response = await dp.Network.getResponseBody({requestId});
    const blobSize = await blobSizePromise;
    if (response.error) {
      testRunner.log(`${url}: error: ${response.error.message}`);
      return;
    }
    const {body, base64Encoded} = response.result;
    const bodySize = base64Encoded ? atob(body).length : body.length;
    testRunner.log(`${url}: blob size: ${blobSize}, body size: ${
        bodySize}, base64Encoded: ${base64Encoded}`);
  }

  await logResponseBody(testRunner.url('./resources/final.js'));
  await logResponseBody(testRunner.url('./resources/abe.png'));

  testRunner.completeTest();
})

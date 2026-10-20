(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that aborting a fetch whose response is consumed as a blob fails the request.`);

  await dp.Network.enable();
  const events = [];
  dp.Network.onLoadingFinished(() => events.push('loadingFinished'));
  dp.Network.onLoadingFailed(event => {
    const {errorText, canceled} = event.params;
    events.push(`loadingFailed: ${errorText}, canceled: ${canceled}`);
  });

  // Sends the headers with the first bytes and the rest of the body half a
  // second later.
  const url = '/inspector-protocol/resources/data-xfer-resource.php?size=1000' +
      '&flush_header_with_x_bytes=100&flush_every=1000' +
      '&wait_after_headers_packet=500';

  async function fetchAndAbort(description, abortBeforeBlob) {
    const failedPromise = dp.Network.onceLoadingFailed();
    const result = await session.evaluateAsync(async (url, abortBeforeBlob) => {
      const controller = new AbortController();
      const response = await fetch(url, {signal: controller.signal});
      if (abortBeforeBlob)
        controller.abort();
      const blobPromise = response.blob();
      controller.abort();
      return blobPromise.then(blob => 'blob size: ' + blob.size, e => e.name);
    }, url, abortBeforeBlob);
    await failedPromise;
    testRunner.log(`${description}: ${result}`);
  }

  await fetchAndAbort('Abort before response.blob()', true);
  await fetchAndAbort('Abort during body download', false);

  // Shows that the body from this URL does go to the blob, and gives the blob
  // creation for the aborted fetches time to complete.
  const blobSizePromise = session.evaluateAsync(
      url => fetch(url).then(r => r.blob()).then(b => b.size), url);
  const requestId = (await dp.Network.onceRequestWillBeSent()).params.requestId;
  await dp.Network.onceLoadingFinished(e => e.params.requestId === requestId);
  const {body} = (await dp.Network.getResponseBody({requestId})).result;
  testRunner.log(`No abort: blob size: ${await blobSizePromise}, body size: ${
      body.length}`);

  testRunner.log(events.join('\n'));
  testRunner.completeTest();
})

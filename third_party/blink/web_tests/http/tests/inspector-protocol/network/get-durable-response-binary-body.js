(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that binary response bodies are correctly base64 encoded when fetched via DurableMessages.`);

  await dp.Network.enable({maxTotalBufferSize: 1024, enableDurableMessages: true});

  const url =
    '/inspector-protocol/resources/data-xfer-resource.php' +
    '?size=256&content_type=application/octet-stream&binary_payload=1';
  dp.Runtime.evaluate({expression: `fetch('${url}').then(r => r.text())`});

  const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
  const requestFinishedLoading = (await dp.Network.onceLoadingFinished()).params;
  if (requestWillBeSent.requestId !== requestFinishedLoading.requestId) {
    testRunner.log('Request ID mismatch');
  }
  testRunner.log(requestFinishedLoading);
  const {result} = await dp.Network.getResponseBody({requestId: requestWillBeSent.requestId});

  testRunner.log(`Body is base64 encoded: ${result.base64Encoded}`);

  // The PHP script generates a sequence of bytes from 0 to 255.
  // Verify the base64-decoded content matches that.
  const originalBinaryString = Array.from({length: 256}, (_, i) => String.fromCharCode(i)).join('');

  testRunner.log('Expected (base64): ' + btoa(originalBinaryString));
  testRunner.log('Received (base64): ' + result.body);
  if (atob(result.body) == originalBinaryString) {
    testRunner.log('Decoded body matches.');
  }
  testRunner.completeTest();
})
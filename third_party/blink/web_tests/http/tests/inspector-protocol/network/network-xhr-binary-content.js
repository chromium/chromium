// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that binary XHR response is not corrupted.');

  await dp.Network.enable();

  const url = '/inspector-protocol/resources/data-xfer-resource.php' +
      '?size=256&content_type=application/octet-stream&binary_payload=1';

  let failed = false;
  dp.Network.onLoadingFailed(() => {
    failed = true;
  });

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  const loadingFinishedPromise = dp.Network.onceLoadingFinished();

  const xhrStatus = await dp.Runtime.evaluate({
    expression: `new Promise(resolve => {
      const xhr = new XMLHttpRequest();
      xhr.responseType = '';
      xhr.onreadystatechange = () => {
        if (xhr.readyState === XMLHttpRequest.DONE) {
          resolve(xhr.status);
        }
      };
      xhr.open('GET', ${JSON.stringify(url)}, true);
      xhr.withCredentials = false;
      xhr.send(null);
    })`,
    awaitPromise: true,
    returnByValue: true,
  });

  const [requestWillBeSent, responseReceived, loadingFinished] =
      await Promise.all([
        requestWillBeSentPromise,
        responseReceivedPromise,
        loadingFinishedPromise,
      ]);

  const requestId = requestWillBeSent.params.requestId;
  if (failed || responseReceived.params.requestId !== requestId ||
      loadingFinished.params.requestId !== requestId ||
      xhrStatus.result.result.value !== 200 ||
      responseReceived.params.response.status !== 200) {
    testRunner.fail('Binary XHR failed or mismatched request ID.');
    return;
  }

  testRunner.log('request.type: ' + responseReceived.params.type);
  testRunner.log('request.mimeType: ' +
                 responseReceived.params.response.mimeType);

  const {result: {base64Encoded, body}} =
      await dp.Network.getResponseBody({requestId});

  testRunner.log('request.base64Encoded: ' + base64Encoded);
  testRunner.log('request.base64: ' + body);

  const raw = atob(body);
  const bytes = [];
  for (let i = 0; i < raw.length; ++i) {
    bytes.push(raw.charCodeAt(i));
  }
  testRunner.log('request.content decoded: ' + bytes.join(', '));
  testRunner.completeTest();
});

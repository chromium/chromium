// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests XHR network resource type and size for asynchronous requests when "blob" is specified as the response type.\n');

  await dp.Network.enable();

  const url = testRunner.url('resources/get-response-body-valid-text.php');
  let requestId = null;
  let failed = false;
  let resourceSize = 0;

  const responseReceivedPromise = dp.Network.onceResponseReceived(
      event => requestId !== null && event.params.requestId === requestId);
  const loadingFinishedPromise = dp.Network.onceLoadingFinished(
      event => requestId !== null && event.params.requestId === requestId);

  dp.Network.onRequestWillBeSent(event => {
    if (event.params.request.url === url) {
      requestId = event.params.requestId;
    }
  });
  dp.Network.onDataReceived(event => {
    if (requestId !== null && event.params.requestId === requestId) {
      resourceSize += event.params.dataLength;
    }
  });
  dp.Network.onLoadingFailed(event => {
    if (requestId !== null && event.params.requestId === requestId) {
      failed = true;
    }
  });

  const xhrResult = await session.evaluateAsync(xhrUrl => {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.responseType = 'blob';
      xhr.onreadystatechange = () => {
        if (xhr.readyState === XMLHttpRequest.DONE) {
          resolve({
            status: xhr.status,
            responseType: xhr.responseType,
            isBlob: xhr.response instanceof Blob,
            blobSize: xhr.response instanceof Blob ? xhr.response.size : -1,
          });
        }
      };
      xhr.onerror = () => reject(new Error('XHR failed'));
      xhr.open('GET', xhrUrl, true);
      xhr.withCredentials = false;
      xhr.send(null);
    });
  }, url);

  const [responseReceived] =
      await Promise.all([responseReceivedPromise, loadingFinishedPromise]);

  testRunner.log(responseReceived.params.response.url);
  testRunner.log('xhr.responseType: ' + xhrResult.responseType);
  testRunner.log('xhr.response instanceof Blob: ' + xhrResult.isBlob);
  testRunner.log('xhr.response.size: ' + xhrResult.blobSize);
  testRunner.log('resource.type: ' +
                 responseReceived.params.type.toLowerCase());
  testRunner.log('resource.size: ' + resourceSize);
  if (failed || xhrResult.status !== 200 ||
      responseReceived.params.response.status !== 200) {
    testRunner.fail(`Resource loading failed (failed=${failed}, xhrStatus=${
        xhrResult.status}, cdpStatus=${
        responseReceived.params.response.status}).`);
    return;
  }

  const getBodyResponse = await dp.Network.getResponseBody({requestId});
  if (getBodyResponse.error) {
    testRunner.fail(`Network.getResponseBody failed: ${
        JSON.stringify(getBodyResponse.error)}`);
    return;
  }
  const {body, base64Encoded} = getBodyResponse.result;
  testRunner.log('base64Encoded: ' + base64Encoded);
  testRunner.log('resource.content after requesting content: ' + body);
  testRunner.completeTest();
});

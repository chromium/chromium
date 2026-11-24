// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests cyrillic xhr content is correctly loaded in inspector.\n`);

  await dp.Runtime.enable();
  await dp.Network.enable();

  dp.Runtime.onExceptionThrown(event => {
    testRunner.fail('Uncaught exception: ' + JSON.stringify(event.params));
  });

  let failed = false;
  dp.Network.onLoadingFailed(event => {
    failed = true;
    testRunner.log('Loading failed: ' + JSON.stringify(event.params));
  });

  async function runXHR(url, async) {
    const requestUrl = new URL(url).href;
    const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent(
        e => e.params.request.url === requestUrl);
    const responseReceivedPromise = dp.Network.onceResponseReceived(
        e => e.params.response.url === requestUrl);
    const loadingFinishedPromise = dp.Network.onceLoadingFinished();

    const xhrResult = await session.evaluateAsync(`
      new Promise(resolve => {
        const xhr = new XMLHttpRequest();
        xhr.responseType = '';
        xhr.onreadystatechange = function() {
          if (xhr.readyState === XMLHttpRequest.DONE) {
            resolve({status: xhr.status, responseText: xhr.responseText});
          }
        };
        xhr.open('GET', ${JSON.stringify(requestUrl)}, ${
        JSON.stringify(async)});
        xhr.withCredentials = false;
        xhr.send(null);
      })
    `);

    const [requestWillBeSent, responseReceived, loadingFinished] =
        await Promise.all([
          requestWillBeSentPromise,
          responseReceivedPromise,
          loadingFinishedPromise,
        ]);

    if (xhrResult.status !== 200) {
      testRunner.fail(
          `Unexpected XHR status for ${requestUrl}: ${xhrResult.status}`);
    }
    if (requestWillBeSent.params.requestId !==
            loadingFinished.params.requestId ||
        responseReceived.params.requestId !==
            requestWillBeSent.params.requestId) {
      testRunner.fail(`Mismatched requestId for ${requestUrl}`);
    }
    return {
      requestId: requestWillBeSent.params.requestId,
      url: responseReceived.params.response.url,
      type: responseReceived.params.type,
      status: responseReceived.params.response.status,
      xhrResponseText: xhrResult.responseText,
    };
  }

  const syncRequest =
      await runXHR(testRunner.url('resources/cyrillic.html?sync'), false);
  const asyncRequest =
      await runXHR(testRunner.url('resources/cyrillic.html?async'), true);

  if (failed) {
    testRunner.fail('Resource loading failed.');
  }

  for (const request of [syncRequest, asyncRequest]) {
    if (request.type !== 'XHR' || request.status !== 200) {
      testRunner.fail(`Unexpected response metadata for ${request.url}: ` +
          `type=${request.type} status=${request.status}`);
    }
    const response = await dp.Network.getResponseBody({
      requestId: request.requestId,
    });
    if (response.error) {
      testRunner.fail(`Network.getResponseBody failed for ${request.url}: ` +
          JSON.stringify(response.error));
    }
    if (response.result.base64Encoded) {
      testRunner.fail(
          `Expected text response body for ${request.url}, got base64`);
    }
    if (response.result.body !== request.xhrResponseText) {
      testRunner.fail(
          `Network.getResponseBody mismatch vs xhr.responseText for ${
              request.url}`);
    }
    testRunner.log(request.url);
    testRunner.log('resource.content: ' + response.result.body);
  }

  testRunner.completeTest();
});

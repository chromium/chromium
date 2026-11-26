// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests empty xhr content is correctly loaded in inspector.');
  await dp.Network.enable();

  dp.Network.onLoadingFailed(event => {
    testRunner.fail(
        `Unexpected Network.loadingFailed: ${JSON.stringify(event.params)}`);
  });

  async function sendEmptyXHR(url, async) {
    let requestId;
    const requestSentPromise = dp.Network.onceRequestWillBeSent(event => {
      if (event.params.request.url === url) {
        requestId = event.params.requestId;
        return true;
      }
      return false;
    });
    const responseReceivedPromise = dp.Network.onceResponseReceived(event => {
      return Boolean(requestId) && event.params.requestId === requestId;
    });
    const loadingFinishedPromise = dp.Network.onceLoadingFinished(event => {
      return Boolean(requestId) && event.params.requestId === requestId;
    });

    const evalPromise = async ? session.evaluateAsync((requestUrl) => {
      return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', requestUrl, true);
        xhr.onload = () => resolve(xhr.status);
        xhr.onerror = () => reject(new Error('Async XHR failed'));
        xhr.onabort = () => reject(new Error('Async XHR aborted'));
        xhr.send();
      });
    }, url) : session.evaluate((requestUrl) => {
      const xhr = new XMLHttpRequest();
      xhr.open('GET', requestUrl, false);
      xhr.send();
      return xhr.status;
    }, url);

    const [xhrStatus, requestEvent, responseEvent, loadingEvent] =
        await Promise.all([
          evalPromise,
          requestSentPromise,
          responseReceivedPromise,
          loadingFinishedPromise,
        ]);

    if (xhrStatus !== 200 || responseEvent.params.response.status !== 200 ||
        responseEvent.params.type !== 'XHR' ||
        loadingEvent.params.encodedBodyLength !== 0) {
      testRunner.fail(
          `Unexpected response for ${url}: xhrStatus=${xhrStatus}, ` +
          `status=${responseEvent.params.response.status}, ` +
          `type=${responseEvent.params.type}, ` +
          `encodedBodyLength=${loadingEvent.params.encodedBodyLength}`);
      return null;
    }
    return {requestId, url: requestEvent.params.request.url};
  }

  async function dumpRequest(request) {
    if (!request) {
      return;
    }
    testRunner.log(request.url);
    const bodyResponse =
        await dp.Network.getResponseBody({requestId: request.requestId});
    if (bodyResponse.error) {
      testRunner.fail(`Network.getResponseBody failed: ${
          JSON.stringify(bodyResponse.error)}`);
      return;
    }
    if (bodyResponse.result.base64Encoded) {
      testRunner.fail('Expected non-base64 empty response body');
      return;
    }
    testRunner.log(
        `resource.content: ${JSON.stringify(bodyResponse.result.body)}`);
  }

  const syncUrl = testRunner.url('resources/content-length-0.pl?sync');
  const asyncUrl = testRunner.url('resources/content-length-0.pl?async');

  const syncRequest = await sendEmptyXHR(syncUrl, false);
  const asyncRequest = await sendEmptyXHR(asyncUrl, true);

  await dumpRequest(syncRequest);
  await dumpRequest(asyncRequest);

  testRunner.completeTest();
})

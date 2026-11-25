// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests disabling cache from inspector.');

  await dp.Network.enable();

  dp.Network.onLoadingFailed(event => {
    testRunner.log(`FAIL: loadingFailed: ${JSON.stringify(event.params)}`);
  });

  const finishedRequests = new Set();
  const loadingWaiters = new Map();
  dp.Network.onLoadingFinished(event => {
    const {requestId} = event.params;
    const resolve = loadingWaiters.get(requestId);
    if (resolve) {
      loadingWaiters.delete(requestId);
      resolve();
    } else {
      finishedRequests.add(requestId);
    }
  });

  function waitForLoadingFinished(requestId) {
    if (finishedRequests.has(requestId)) {
      finishedRequests.delete(requestId);
      return Promise.resolve();
    }
    return new Promise(resolve => loadingWaiters.set(requestId, resolve));
  }

  const url = testRunner.url('resources/cached.php');

  async function fetchXHR() {
    const responsePromise =
        dp.Network.onceResponseReceived(event => event.params.type === 'XHR' &&
                                            event.params.response.url === url);
    const xhrStatus = await session.evaluateAsync(
        requestUrl => new Promise((resolve, reject) => {
          const xhr = new XMLHttpRequest();
          xhr.open('GET', requestUrl, true);
          xhr.withCredentials = false;
          xhr.onload = () => resolve(xhr.status);
          xhr.onerror = () => reject(new Error('XHR failed'));
          xhr.send(null);
        }),
        url);
    const responseReceived = await responsePromise;
    const {requestId, type, response} = responseReceived.params;
    await waitForLoadingFinished(requestId);
    const {body, base64Encoded} =
        (await dp.Network.getResponseBody({requestId})).result;
    if (xhrStatus !== 200 || response.status !== 200 || type !== 'XHR' ||
        base64Encoded || typeof body !== 'string' ||
        !body.includes('Today\'s lucky number:')) {
      testRunner.fail(`Unexpected response: xhrStatus=${xhrStatus}, ` +
          `status=${response.status}, type=${type}, ` +
          `base64Encoded=${base64Encoded}, body=${body}`);
    }
    testRunner.log(response.url);
    return body;
  }

  const body1 = await fetchXHR();
  const body2 = await fetchXHR();

  await dp.Network.setCacheDisabled({cacheDisabled: true});
  const body3 = await fetchXHR();

  testRunner.log(`First and second resources are equal: ${body1 === body2}`);
  testRunner.log(`Second and third resources differ: ${body2 !== body3}`);

  await dp.Network.setCacheDisabled({cacheDisabled: false});
  testRunner.completeTest();
});

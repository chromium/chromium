// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that XHR redirects preserve request body.');

  await dp.Network.enable();

  const requests = [];
  const activeHopByRequestId = new Map();

  function getHeaderCaseInsensitive(headers, name) {
    const target = name.toLowerCase();
    for (const [key, value] of Object.entries(headers || {})) {
      if (key.toLowerCase() === target) {
        return value;
      }
    }
    return undefined;
  }

  dp.Network.onRequestWillBeSent(event => {
    const {requestId, request, redirectResponse} = event.params;
    const previousHop = activeHopByRequestId.get(requestId);
    if (previousHop && redirectResponse) {
      previousHop.actualMethod =
          getHeaderCaseInsensitive(redirectResponse.headers, 'request-method');
    }
    const hop = {
      method: request.method,
      url: request.url,
      postData: request.postData || '',
      actualMethod: undefined,
    };
    requests.push(hop);
    activeHopByRequestId.set(requestId, hop);
  });

  dp.Network.onResponseReceived(event => {
    const {requestId, response} = event.params;
    const hop = activeHopByRequestId.get(requestId);
    if (hop) {
      hop.actualMethod =
          getHeaderCaseInsensitive(response.headers, 'request-method');
    }
  });

  async function sendPostXhr(url, payload) {
    const loadingFinished = dp.Network.onceLoadingFinished();
    await session.evaluateAsync((targetUrl, body) => {
      return new Promise(resolve => {
        const xhr = new XMLHttpRequest();
        xhr.onreadystatechange = () => {
          if (xhr.readyState === XMLHttpRequest.DONE) {
            resolve();
          }
        };
        xhr.open('POST', targetUrl, true);
        xhr.send(body);
      });
    }, url, payload);
    await loadingFinished;
  }

  await sendPostXhr(
      testRunner.url('resources/redirect-body.cgi?status=301&ttl=1'), 'LOST');
  await sendPostXhr(
      testRunner.url('resources/redirect-body.cgi?status=307&ttl=1'),
      'PRESERVED');

  for (const request of requests) {
    testRunner.log(`${request.method} ${request.url}`);
    testRunner.log(`  actual http method was: ${request.actualMethod}`);
    testRunner.log(`  request body: [${request.postData}]`);
    testRunner.log('');
  }

  testRunner.completeTest();
})

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that XHR redirects preserve http-method.\n');

  await dp.Network.enable();

  function getHeader(headers, targetName) {
    for (const [name, value] of Object.entries(headers || {})) {
      if (name.toLowerCase() === targetName.toLowerCase()) {
        return value;
      }
    }
    return undefined;
  }

  let xhrRequestId = null;
  const hops = [];

  dp.Network.onRequestWillBeSent(event => {
    const {requestId, request, redirectResponse} = event.params;
    if (!xhrRequestId && request.url.includes('redirect-method.cgi')) {
      xhrRequestId = requestId;
    }
    if (requestId !== xhrRequestId) {
      return;
    }
    if (redirectResponse && hops.length > 0) {
      hops[hops.length - 1].actualMethod =
          getHeader(redirectResponse.headers, 'request-method');
    }
    hops.push({
      method: request.method,
      url: request.url,
      actualMethod: undefined,
    });
  });

  dp.Network.onResponseReceived(event => {
    if (event.params.requestId !== xhrRequestId || hops.length === 0) {
      return;
    }
    hops[hops.length - 1].actualMethod =
        getHeader(event.params.response.headers, 'request-method');
  });

  dp.Network.onLoadingFailed(event => {
    if (event.params.requestId === xhrRequestId) {
      testRunner.fail(`Request failed: ${event.params.errorText}`);
    }
  });

  const loadingFinishedPromise =
      dp.Network.onceLoadingFinished(e => e.params.requestId === xhrRequestId);

  const url =
      'http://127.0.0.1:8000/inspector-protocol/network/resources/redirect-method.cgi?status=301&ttl=3';
  await session.evaluateAsync(`
    new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.onload = resolve;
      xhr.onerror = () => reject(new Error('XHR error'));
      xhr.open('PUT', '${url}', true);
      xhr.send(null);
    });
  `);

  await loadingFinishedPromise;

  for (const hop of hops) {
    testRunner.log(`${hop.method} ${hop.url}`);
    testRunner.log(`  actual http method was: ${hop.actualMethod}`);
    testRunner.log('');
  }
  testRunner.completeTest();
});

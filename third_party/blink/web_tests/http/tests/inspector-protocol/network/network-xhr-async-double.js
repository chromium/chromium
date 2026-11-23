// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests responses in network tab for two XHRs sent without any delay ' +
      'between them. Bug 91630\n');

  await dp.Network.enable();

  const url = testRunner.url('./resources/post-echo.pl');
  const requests = [];
  const responses = new Map();
  const finishedIds = new Set();

  let resolveDone;
  const bothFinishedPromise = new Promise(resolve => {
    resolveDone = resolve;
  });

  function checkFinished() {
    if (requests.length === 2 && finishedIds.has(requests[0].requestId) &&
        finishedIds.has(requests[1].requestId)) {
      resolveDone();
    }
  }

  dp.Network.onRequestWillBeSent(event => {
    requests.push(event.params);
    checkFinished();
  });

  dp.Network.onResponseReceived(event => {
    responses.set(event.params.requestId, event.params);
  });

  dp.Network.onLoadingFinished(event => {
    finishedIds.add(event.params.requestId);
    checkFinished();
  });

  dp.Network.onLoadingFailed(event => {
    testRunner.fail(
        `Request ${event.params.requestId} failed: ${event.params.errorText}`);
  });

  const evalResult = await dp.Runtime.evaluate({
    expression: `(() => {
      function makeXHR(method, url, async, payload) {
        return new Promise((resolve, reject) => {
          const xhr = new XMLHttpRequest();
          xhr.responseType = '';
          xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
              if (xhr.status === 200) {
                resolve(xhr.responseText);
              } else {
                reject(new Error('XHR failed with status ' + xhr.status));
              }
            }
          };
          xhr.open(method, url, async);
          xhr.withCredentials = false;
          xhr.send(payload);
        });
      }
      return Promise.all([
        makeXHR('POST', ${JSON.stringify(url)}, true, 'request1'),
        makeXHR('POST', ${JSON.stringify(url)}, true, 'request2'),
      ]);
    })()`,
    awaitPromise: true,
    returnByValue: true,
  });

  if (evalResult.exceptionDetails) {
    testRunner.fail(
        `Evaluation failed: ${JSON.stringify(evalResult.exceptionDetails)}`);
    return;
  }

  await bothFinishedPromise;

  if (requests.length !== 2) {
    testRunner.fail(`Expected 2 requests, got ${requests.length}`);
    return;
  }

  const [request1, request2] = requests;
  if (request1.requestId === request2.requestId) {
    testRunner.fail('Expected distinct requestIds for concurrent XHRs');
    return;
  }

  for (const [index, req] of requests.entries()) {
    const resp = responses.get(req.requestId);
    if (!resp) {
      testRunner.fail(`Missing responseReceived for request ${index + 1}`);
      return;
    }
    if (req.type !== 'XHR' || resp.type !== 'XHR') {
      testRunner.fail(`Expected XHR type for request ${index + 1}, ` +
          `got ${req.type}/${resp.type}`);
      return;
    }
    if (req.request.method !== 'POST' || resp.response.status !== 200) {
      testRunner.fail(`Unexpected method/status for request ${index + 1}: ` +
          `${req.request.method} ${resp.response.status}`);
      return;
    }
  }

  if (request1.request.postData !== 'request1' ||
      request2.request.postData !== 'request2') {
    testRunner.fail(
        `Unexpected request postData: ${request1.request.postData}, ` +
        `${request2.request.postData}`);
    return;
  }

  const [body1Response, body2Response] = await Promise.all([
    dp.Network.getResponseBody({requestId: request1.requestId}),
    dp.Network.getResponseBody({requestId: request2.requestId}),
  ]);
  if (body1Response.error || body2Response.error) {
    testRunner.fail(
        `getResponseBody error: ` +
        `${JSON.stringify(body1Response.error || body2Response.error)}`);
    return;
  }

  const content1 = body1Response.result.body;
  const content2 = body2Response.result.body;
  testRunner.log('resource1.content: ' + content1);
  testRunner.log('resource2.content: ' + content2);

  if (body1Response.result.base64Encoded ||
      body2Response.result.base64Encoded || content1 !== 'request1' ||
      content2 !== 'request2') {
    testRunner.fail(`Unexpected response bodies: ${content1} ` +
        `(b64=${body1Response.result.base64Encoded}), ${content2} ` +
        `(b64=${body2Response.result.base64Encoded})`);
    return;
  }

  testRunner.completeTest();
});

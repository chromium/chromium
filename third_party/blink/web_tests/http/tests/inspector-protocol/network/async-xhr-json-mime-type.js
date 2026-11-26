// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that the content of resources with JSON MIME types can be ' +
      'accessed when loaded by asynchronous XHR requests (Bug 80684) or ' +
      'within iframes/documents.');

  await dp.Network.enable();

  async function loadAndVerify(url, expression) {
    const responseReceivedPromise = dp.Network.onceResponseReceived(
        event => event.params.response.url === url);
    const finishedRequests = new Set();
    const failedRequests = new Map();
    let targetRequestId = null;
    let resolveDone;
    const donePromise = new Promise(resolve => {
      resolveDone = resolve;
    });

    function onLoadingFinished(event) {
      const {requestId} = event.params;
      if (requestId === targetRequestId) {
        resolveDone();
      } else {
        finishedRequests.add(requestId);
      }
    }

    function onLoadingFailed(event) {
      const {requestId, errorText} = event.params;
      failedRequests.set(requestId, errorText);
      if (requestId === targetRequestId) {
        resolveDone();
      }
    }

    dp.Network.onLoadingFinished(onLoadingFinished);
    dp.Network.onLoadingFailed(onLoadingFailed);
    try {
      const evalPromise = session.evaluateAsync(expression);
      const responseReceived = await responseReceivedPromise;
      const {requestId, type, response} = responseReceived.params;
      targetRequestId = requestId;
      if (!finishedRequests.has(requestId) && !failedRequests.has(requestId)) {
        await donePromise;
      }
      await evalPromise;

      testRunner.log('');
      testRunner.log('request.url: ' + response.url);
      testRunner.log('request.type: ' + type);
      testRunner.log('request.mimeType: ' + response.mimeType);
      if (failedRequests.has(requestId)) {
        testRunner.log('request.failed: ' + failedRequests.get(requestId));
        return;
      }
      const {result, error} = await dp.Network.getResponseBody({requestId});
      if (error) {
        testRunner.log('request.error: ' + error.message);
      } else {
        testRunner.log('request.base64Encoded: ' + result.base64Encoded);
        testRunner.log('request.content: ' + result.body);
      }
    } finally {
      dp.Network.offLoadingFinished(onLoadingFinished);
      dp.Network.offLoadingFailed(onLoadingFailed);
    }
  }

  async function testType(contentType) {
    const encodedContentType = encodeURIComponent(contentType);
    const url = testRunner.url('resources/json.php?type=' + encodedContentType);

    await loadAndVerify(url, `
      new Promise(resolve => {
        const xhr = new XMLHttpRequest();
        xhr.responseType = '';
        xhr.onreadystatechange = () => {
          if (xhr.readyState === XMLHttpRequest.DONE) {
            resolve();
          }
        };
        xhr.open('GET', ${JSON.stringify(url)}, true);
        xhr.withCredentials = false;
        xhr.send(null);
      })
    `);

    await loadAndVerify(url, `
      new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.src = ${JSON.stringify(url)};
        iframe.onload = resolve;
        document.body.appendChild(iframe);
      })
    `);
  }

  testRunner.runTestSuite([
    async function test1() {
      await testType('application/json');
    },
    async function test2() {
      await testType('application/vnd.document+json');
    },
  ]);
})

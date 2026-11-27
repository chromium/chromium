// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests fetch network resource type and content.');

  await dp.Network.enable();

  const url = testRunner.url('resources/get-response-body-valid-text.php');
  const requests = [];
  const loadingOutcomes = new Map();
  let targetRequestId = null;
  let resolveOutcome;
  const outcomePromise = new Promise(resolve => {
    resolveOutcome = resolve;
  });

  dp.Network.onRequestWillBeSent(event => requests.push(event.params));
  dp.Network.onLoadingFinished(event => {
    loadingOutcomes.set(event.params.requestId, {failed: false});
    if (event.params.requestId === targetRequestId) {
      resolveOutcome({failed: false});
    }
  });
  dp.Network.onLoadingFailed(event => {
    const outcome = {failed: true, errorText: event.params.errorText};
    loadingOutcomes.set(event.params.requestId, outcome);
    if (event.params.requestId === targetRequestId) {
      resolveOutcome(outcome);
    }
  });

  const requestWillBeSentPromise =
      dp.Network.onceRequestWillBeSent(e => e.params.request.url === url);
  const responseReceivedPromise =
      dp.Network.onceResponseReceived(e => e.params.response.url === url);

  const evalPromise = dp.Runtime.evaluate({
    expression: `fetch(${JSON.stringify(url)}, {}).then(res => res.text())`,
    awaitPromise: true,
  });

  const {params: requestParams} = await requestWillBeSentPromise;
  targetRequestId = requestParams.requestId;
  const outcome =
      loadingOutcomes.get(targetRequestId) ?? (await outcomePromise);

  const evalResult = await evalPromise;
  if (evalResult.error || evalResult.result.exceptionDetails) {
    testRunner.fail(
        `Unexpected evaluation error: ${JSON.stringify(evalResult)}`);
  }
  if (requests.length !== 1) {
    testRunner.fail(`Expected 1 request, got ${requests.length}`);
  }
  if (outcome.failed) {
    testRunner.fail(`Resource loading failed: ${outcome.errorText}`);
  }

  const {params: responseParams} = await responseReceivedPromise;
  if (responseParams.requestId !== targetRequestId) {
    testRunner.fail(
        `Unexpected response requestId: ${responseParams.requestId}`);
  }
  if (requestParams.type !== 'Fetch' || responseParams.type !== 'Fetch') {
    testRunner.fail(`Unexpected resource type: request=${
        requestParams.type}, response=${responseParams.type}`);
  }
  if (responseParams.response.status !== 200) {
    testRunner.fail(
        `Unexpected HTTP status: ${responseParams.response.status}`);
  }

  const responseBody =
      await dp.Network.getResponseBody({requestId: targetRequestId});
  if (responseBody.error) {
    testRunner.fail(
        `getResponseBody failed: ${JSON.stringify(responseBody.error)}`);
  }
  if (responseBody.result.base64Encoded) {
    testRunner.fail('Expected non-base64 text response body.');
  }

  testRunner.log(requestParams.request.url);
  testRunner.log(`resource.type: ${responseParams.type.toLowerCase()}`);
  testRunner.log(
      `resource.content after requesting content: ${responseBody.result.body}`);
  testRunner.completeTest();
});

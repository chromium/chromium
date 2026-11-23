// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests content is available for failed image request.');

  await dp.Network.enable();

  const requests = [];
  dp.Network.onRequestWillBeSent(event => requests.push(event.params));

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  const loadingResultPromise = Promise.race([
    dp.Network.onceLoadingFinished(),
    dp.Network.onceLoadingFailed(),
  ]);

  const imageUrl = testRunner.url('./resources/image-404.php');
  const loadOutcomePromise = session.evaluateAsync(`
    new Promise((resolve, reject) => {
      const image = new Image();
      image.onerror = () => resolve('onerror');
      image.onload = () => reject(new Error('Expected 404 image to fail loading'));
      image.src = ${JSON.stringify(imageUrl)};
    })
  `);

  const [requestWillBeSent, responseReceived, loadingResult, loadOutcome] =
      await Promise.all([
        requestWillBeSentPromise,
        responseReceivedPromise,
        loadingResultPromise,
        loadOutcomePromise,
      ]);

  testRunner.log('image load outcome: ' + loadOutcome);
  testRunner.log(requestWillBeSent.params.request.url);
  testRunner.log('resource.type: ' + responseReceived.params.type);
  testRunner.log('response.status: ' + responseReceived.params.response.status);
  if (loadingResult.method !== 'Network.loadingFinished') {
    testRunner.fail('Expected Network.loadingFinished, got ' +
                    loadingResult.method);
    return;
  }
  testRunner.log('loading event: ' + loadingResult.method);

  testRunner.log('resources count = ' + requests.length);
  for (const request of requests) {
    testRunner.log(request.request.url);
    const bodyResponse =
        await dp.Network.getResponseBody({requestId: request.requestId});
    if (bodyResponse.error) {
      testRunner.fail('getResponseBody failed: ' + bodyResponse.error.message);
      return;
    }
    const content = bodyResponse.result.base64Encoded ?
        atob(bodyResponse.result.body) :
        bodyResponse.result.body;
    testRunner.log('resource.content after requesting content: ' + content);
  }

  testRunner.completeTest();
})

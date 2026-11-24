// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests XHR network resource type and content for synchronous requests. Bug 61205\n`);

  await dp.Runtime.enable();
  await dp.Network.enable();

  const responseReceivedPromise = dp.Network.onceResponseReceived();
  const terminalEventPromise = Promise.race([
    dp.Network.onceLoadingFinished().then(() => ({failed: false})),
    dp.Network.onceLoadingFailed().then(
        event => ({failed: true, errorText: event.params.errorText})),
  ]);

  await dp.Runtime.evaluate({
    expression: `
      (() => {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', '/devtools/network/resources/resource.php', false);
        xhr.withCredentials = false;
        xhr.send(null);
      })()
    `,
    objectGroup: 'console',
  });

  const terminalEvent = await terminalEventPromise;
  if (terminalEvent.failed) {
    testRunner.fail('Resource loading failed: ' + terminalEvent.errorText);
    return;
  }

  const responseReceived = (await responseReceivedPromise).params;
  testRunner.log(responseReceived.response.url);
  testRunner.log('resource.type: ' + responseReceived.type.toLowerCase());

  const bodyResponse =
      await dp.Network.getResponseBody({requestId: responseReceived.requestId});
  if (bodyResponse.error) {
    testRunner.fail('Failed to get response body: ' +
                    bodyResponse.error.message);
    return;
  }
  testRunner.log('resource.content after requesting content: ' +
                 bodyResponse.result.body);
  testRunner.completeTest();
});

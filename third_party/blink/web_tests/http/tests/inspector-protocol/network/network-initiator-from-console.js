// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that there is no javascript error when console evaluation causes resource loading.\n');

  await dp.Page.enable();
  await dp.Network.enable();
  await dp.Runtime.enable();

  const load = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await load;
  testRunner.log('Page reloaded.');

  let exceptionThrown = false;
  dp.Runtime.onExceptionThrown(event => {
    exceptionThrown = true;
    testRunner.fail(`Unexpected exception: ${JSON.stringify(event.params)}`);
  });
  dp.Network.onLoadingFailed(event => {
    testRunner.fail(`Request failed: ${JSON.stringify(event.params)}`);
  });

  const scriptUrl = testRunner.url('resources/silent_script.js');
  let targetRequestId = null;
  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent(event => {
    if (event.params.request.url === scriptUrl) {
      targetRequestId = event.params.requestId;
      return true;
    }
    return false;
  });
  const responseReceivedPromise = dp.Network.onceResponseReceived(
      event => event.params.response.url === scriptUrl);
  const loadingFinishedPromise = dp.Network.onceLoadingFinished(
      event => event.params.requestId === targetRequestId);

  let expression = '';
  expression += 'var s = document.createElement("script");';
  expression += `s.src = ${JSON.stringify(scriptUrl)};`;
  expression += 'document.head.appendChild(s);';
  const evalResult = await dp.Runtime.evaluate({
    expression,
    objectGroup: 'console',
  });
  if (evalResult.error || evalResult.result?.exceptionDetails) {
    testRunner.fail(`Console evaluation failed: ${JSON.stringify(evalResult)}`);
    return;
  }

  const [requestWillBeSent, responseReceived] = await Promise.all([
    requestWillBeSentPromise,
    responseReceivedPromise,
    loadingFinishedPromise,
  ]);
  const {request, initiator} = requestWillBeSent.params;
  const {response, type} = responseReceived.params;
  const topFrame = initiator.stack?.callFrames?.[0];

  testRunner.log(`request.url: ${request.url}`);
  testRunner.log(`resource.type: ${type}`);
  testRunner.log(`response.status: ${response.status}`);
  testRunner.log(`initiator.type: ${initiator.type}`);
  testRunner.log(`initiator.stack.callFrames.length > 0: ${
      Boolean(initiator.stack?.callFrames?.length)}`);
  testRunner.log(`topFrame.url: ${JSON.stringify(topFrame?.url)}`);
  testRunner.log(`topFrame.lineNumber: ${topFrame?.lineNumber}`);
  testRunner.log(`exceptionThrown: ${exceptionThrown}`);
  testRunner.completeTest();
})

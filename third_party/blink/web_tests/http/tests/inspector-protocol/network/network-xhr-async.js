// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests XHR network resource type and content for asynchronous requests. Bug 61205\n`);

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Network.enable();

  await dp.Runtime.evaluate({
    expression: `
  let lastXHRIndex = 0;

  function xhrLoadedCallback() {
    console.log('XHR loaded: ' + ++lastXHRIndex);
  }

  function makeSimpleXHR(method, url, async) {
    let xhr = new XMLHttpRequest();
    xhr.responseType = '';
    xhr.onreadystatechange = function() {
      if (xhr.readyState === XMLHttpRequest.DONE) {
        xhrLoadedCallback();
      }
    };
    xhr.open(method, url, async);
    xhr.withCredentials = false;
    xhr.send(null);
  }
`,
    objectGroup: 'console',
  });

  let failed = false;
  dp.Network.onLoadingFailed(() => {
    failed = true;
  });

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const requestWillBeSentExtraInfoPromise =
      dp.Network.onceRequestWillBeSentExtraInfo();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  const responseReceivedExtraInfoPromise =
      dp.Network.onceResponseReceivedExtraInfo();
  const dataReceivedPromise = dp.Network.onceDataReceived();
  const loadingFinishedPromise = dp.Network.onceLoadingFinished();
  const consolePromise = dp.Runtime.onceConsoleAPICalled();

  await dp.Runtime.evaluate({
    expression:
        `makeSimpleXHR('GET', 'http://127.0.0.1:8000/devtools/network/resources/resource.php', true)`,
    objectGroup: 'console',
  });

  const [requestWillBeSent, , responseReceived, , , , consoleEvent] =
      await Promise.all([
        requestWillBeSentPromise,
        requestWillBeSentExtraInfoPromise,
        responseReceivedPromise,
        responseReceivedExtraInfoPromise,
        dataReceivedPromise,
        loadingFinishedPromise,
        consolePromise,
      ]);

  const requestId = requestWillBeSent.params.requestId;
  testRunner.log(responseReceived.params.response.url);
  testRunner.log('resource.type: ' +
                 responseReceived.params.type.toLowerCase());
  if (failed) {
    testRunner.fail('Resource loading failed.');
    return;
  }

  const {result: {body}} = await dp.Network.getResponseBody({requestId});
  testRunner.log('resource.content after requesting content: ' + body);

  const frame = consoleEvent.params.stackTrace.callFrames[0];
  testRunner.log(
      `VM:${frame.lineNumber + 1} ${consoleEvent.params.args[0].value}`);
  testRunner.completeTest();
});

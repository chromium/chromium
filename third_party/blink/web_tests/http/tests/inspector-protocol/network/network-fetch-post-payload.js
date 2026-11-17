// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests fetch() network resource payload is not corrupted by transcoding.');

  await dp.Network.enable({maxPostDataSize: 65536});

  const payload = '\u201AFoo\u201B';
  const url = testRunner.url('./resources/post-echo.pl?foo');

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const requestWillBeSentExtraInfoPromise =
      dp.Network.onceRequestWillBeSentExtraInfo();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  const responseReceivedExtraInfoPromise =
      dp.Network.onceResponseReceivedExtraInfo();

  const evalPromise = dp.Runtime.evaluate({
    expression: `fetch(${JSON.stringify(url)}, {method: 'POST', body: ${
        JSON.stringify(payload)}}).then(r => r.text())`,
    objectGroup: 'console',
    includeCommandLineAPI: false,
    awaitPromise: true,
  });

  const [{params: {requestId, request}}] = await Promise.all([
    requestWillBeSentPromise,
    requestWillBeSentExtraInfoPromise,
    responseReceivedPromise,
    responseReceivedExtraInfoPromise,
    evalPromise,
  ]);

  const {result: {postData}} = await dp.Network.getRequestPostData({requestId});

  testRunner.log(request.url);
  testRunner.log(`Query string: ${new URL(request.url).search.slice(1)}`);
  testRunner.log(`requestWillBeSent postData matches payload: ${
      request.postData === payload} (${request.postData})`);
  testRunner.log(`getRequestPostData matches payload: ${
      postData === payload} (${postData})`);
  testRunner.completeTest();
});

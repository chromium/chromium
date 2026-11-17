// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests XHR network resource payload is not corrupted by transcoding.');

  await dp.Network.enable({maxPostDataSize: 65536});

  const payload = '\u201AFoo\u201B';
  const url = testRunner.url('./resources/post-echo.pl?foo');

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const loadingFinishedPromise = dp.Network.onceLoadingFinished();

  await dp.Runtime.evaluate({
    expression: `new Promise(resolve => {
      const xhr = new XMLHttpRequest();
      xhr.responseType = '';
      xhr.onreadystatechange = () => {
        if (xhr.readyState === XMLHttpRequest.DONE) {
          resolve();
        }
      };
      xhr.open('POST', ${JSON.stringify(url)}, true);
      xhr.withCredentials = false;
      xhr.send(${JSON.stringify(payload)});
    })`,
    objectGroup: 'console',
    awaitPromise: true,
  });

  const {params: {requestId, request}} = await requestWillBeSentPromise;
  await loadingFinishedPromise;
  const {result: {postData}} = await dp.Network.getRequestPostData({requestId});

  testRunner.log(request.url);
  testRunner.log(`queryString: ${new URL(request.url).search.slice(1)}`);
  testRunner.log(`hasPostData: ${request.hasPostData}`);
  testRunner.log(
      `request.postData === payload: ${request.postData === payload}`);
  testRunner.log(`request.postData: ${request.postData}`);
  testRunner.log(
      `request.postDataEntries: ${JSON.stringify(request.postDataEntries)}`);
  testRunner.log(`getRequestPostData === payload: ${postData === payload}`);
  testRunner.log(`getRequestPostData: ${postData}`);
  testRunner.completeTest();
});

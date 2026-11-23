// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that a used font-face is reported and an unused font-face is not reported.');
  await dp.Network.enable();

  const requestsById = new Map();
  const startedRequests = [];
  const finishedRequests = [];
  let resolveDone;
  const donePromise = new Promise(resolve => {
    resolveDone = resolve;
  });

  dp.Network.onRequestWillBeSent(event => {
    const name = new URL(event.params.request.url).pathname.split('/').pop();
    if (name === 'done') {
      resolveDone();
      return;
    }
    requestsById.set(event.params.requestId, name);
    startedRequests.push(`RequestStarted: ${name}`);
  });

  function onRequestFinished(event) {
    const name = requestsById.get(event.params.requestId);
    if (name) {
      requestsById.delete(event.params.requestId);
      finishedRequests.push(`RequestFinished: ${name}`);
    }
  }

  dp.Network.onLoadingFinished(onRequestFinished);
  dp.Network.onLoadingFailed(onRequestFinished);

  await session.navigate('./resources/font-face.html');
  await donePromise;

  startedRequests.sort();
  finishedRequests.sort();
  for (const result of [...startedRequests, ...finishedRequests]) {
    testRunner.log(result);
  }
  testRunner.completeTest();
})

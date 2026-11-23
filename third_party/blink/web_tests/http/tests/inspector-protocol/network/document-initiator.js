// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/document-initiator.html',
      'Tests that page navigation initiated by JS is correctly reported.');

  await dp.Network.enable();
  await dp.Page.enable();

  const matchingRequests = [];
  dp.Network.onRequestWillBeSent(event => {
    if (event.params.request.url.includes('?foo')) {
      matchingRequests.push(event.params);
    }
  });

  const loadPromise = dp.Page.onceLoadEventFired();
  await session.evaluate('navigateFromScript()');
  await loadPromise;

  if (matchingRequests.length !== 1) {
    testRunner.fail(`Expected 1 ?foo request, got ${matchingRequests.length}`);
    return;
  }

  const {request, initiator} = matchingRequests[0];
  testRunner.log(`${request.url}: ${initiator.type}`);
  if (initiator.url) {
    testRunner.log(`    ${initiator.url} ${initiator.lineNumber}`);
  }
  const frame =
      (initiator.stack?.callFrames ?? []).find(frame => frame.lineNumber);
  if (frame) {
    testRunner.log(
        `    ${frame.functionName} ${frame.url} ${frame.lineNumber}`);
  }
  testRunner.completeTest();
})

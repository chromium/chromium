// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests network interception with auth proxy.\n');
  const {result: {sessionId}} =
      await testRunner.browserP().Target.attachToBrowserTarget({});
  const {protocol: bProtocol} = testRunner.createSessionFor(sessionId);

  const proxyServer = testRunner.params('proxy');
  const {result: {browserContextId}} =
      await bProtocol.Target.createBrowserContext({proxyServer});
  const {result: {targetId}} = await bProtocol.Target.createTarget(
      {browserContextId: browserContextId, url: 'about:blank'});

  const {result: {sessionId: targetSessionId}} =
      await bProtocol.Target.attachToTarget({targetId, flatten: true});
  const {protocol: dp} = testRunner.createSessionFor(targetSessionId);

  let authChallengeSeen = false;
  const filesLoaded = new Set();

  await dp.Fetch.enable({handleAuthRequests: true});
  await dp.Page.enable();

  dp.Fetch.onRequestPaused(async event => {
    const url = new URL(event.params.request.url);
    filesLoaded.add(url.pathname);
    dp.Fetch.continueRequest({requestId: event.params.requestId});
  });

  dp.Fetch.onAuthRequired(async event => {
    authChallengeSeen = true;
    dp.Fetch.continueWithAuth({
      requestId: event.params.requestId,
      authChallengeResponse:
          {response: 'ProvideCredentials', username: 'user', password: 'pass'}
    });
  });

  dp.Page.navigate({url: 'http://host.test/dom_tree_test.html'});
  await dp.Page.onceLoadEventFired();

  testRunner.log(`Auth challenge seen: ${authChallengeSeen}`);
  const sortedFiles = Array.from(filesLoaded).sort();
  testRunner.log(`Files loaded:\n  ${sortedFiles.join('\n  ')}`);

  testRunner.completeTest();
})

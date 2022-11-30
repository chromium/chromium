// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that disposing Emulation agent before virtual time expires ' +
      'doesn\'t cause a crash');

  // Create another session, so that we can have it detached while maintaining
  // control of the renderer through the original session.
  const bp = testRunner.browserP();
  const targets = (await bp.Target.getTargets()).result.targetInfos;
  const pageTarget = targets.find(
      target => target.url.endsWith('inspector-protocol-page.html'));
  const sessionId = (await bp.Target.attachToTarget({
      targetId: pageTarget.targetId, flatten: true})).result.sessionId;
  const session2 = new TestRunner.Session(testRunner, sessionId);
  const dp2 = session2.protocol;

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('https://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`<html></html>`));

  // Enable virtual time through the second session.
  await dp2.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'https://test.com/index.html'});
  dp2.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});

  // Pause page with VT paused using request interception, then detach
  // session, then let the time expire.
  const fetchResponse =
      session.evaluateAsync(`fetch('/fetch').then(r => r.text())`);
  const request = await helper.onceRequest('https://test.com/fetch').matched();
  dp.Inspector.onceTargetCrashed(() => {
    testRunner.log('FAIL: target crashed!');
    testRunner.completeTest();
  });
  await bp.Target.detachFromTarget({sessionId});
  await dp.Fetch.fulfillRequest(
      Object.assign(FetchHelper.makeContentResponse(`test fetch`),
      {requestId: request.requestId}));
  testRunner.log(`fetch request: ${await fetchResponse}`);
  // Make a round-trip to the page to make sure it's alive.
  testRunner.log(await session.evaluate('"PASSED"'));
  testRunner.completeTest();
})

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log(
      'Tests context disposal on detach from page exposing DevTools protocol.');

  // Create extra browser session.
  const {result: {sessionId: browserSessionId}} =
      await testRunner.browserP().Target.attachToBrowserTarget({});
  const browserSession = testRunner.createSessionFor(browserSessionId);
  const bp = browserSession.protocol;

  const {result: {browserContextId}} =
      await bp.Target.createBrowserContext({disposeOnDetach: true});

  const {result: {targetId}} = await bp.Target.createTarget({
    browserContextId,
    url: 'about:blank',
  });
  const {result: {sessionId}} =
      await bp.Target.attachToTarget({targetId, flatten: true});
  const session = testRunner.createSessionFor(sessionId);

  await bp.Target.exposeDevToolsProtocol({targetId, bindingName: 'cdp'});

  const testFrameworkURL =
      new URL(
          '/resources/inspector-protocol-test-subtarget.html', location.href)
          .href;
  await session.navigate(testFrameworkURL);

  await session.evaluateAsync(async () => {
    const testRunner = new TestRunner(
        '', '', DevToolsAPI._log, () => {}, DevToolsAPI._fetch, {});
    const bp = testRunner.browserP();
    const promises = [];
    for (let i = 0; i < 64; i++) {
      promises.push(bp.Target.createBrowserContext({disposeOnDetach: true}));
    }
    await Promise.all(promises);
  });

  const contextsBefore =
      (await testRunner.browserP().Target.getBrowserContexts())
          .result.browserContextIds;
  testRunner.log(
      `Browser contexts count before detach: ${contextsBefore.length}`);

  testRunner.log('Detaching browser session...');
  await testRunner.browserP().Target.detachFromTarget(
      {sessionId: browserSessionId});

  const contextsAfter =
      (await testRunner.browserP().Target.getBrowserContexts())
          .result.browserContextIds;
  testRunner.log(
      `Browser contexts count after detach: ${contextsAfter.length}\n`);

  testRunner.completeTest();
});

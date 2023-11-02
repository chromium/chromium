// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  testRunner.log('Tests that headless session can configure proxy.\n');
  const { result: { sessionId } } =
  await testRunner.browserP().Target.attachToBrowserTarget({});
  const { protocol: bProtocol } = new TestRunner.Session(testRunner, sessionId);

  async function dumpWithProxyServer(proxyServer) {
    const { result: { browserContextId } } =
        await bProtocol.Target.createBrowserContext({ proxyServer });
    const { result: { targetId }} =
        await bProtocol.Target.createTarget({
      browserContextId: browserContextId,
      url: 'about:blank'
    });

    const { result: { sessionId } } =
        await bProtocol.Target.attachToTarget({ targetId, flatten: true });
    const { protocol: pProtocol } =
        new TestRunner.Session(testRunner, sessionId);
    await pProtocol.Page.enable({});
    await pProtocol.Page.navigate({
      url: testRunner._testBaseURL + 'resources/title.html'
    });
    await pProtocol.Page.onceLoadEventFired();
    const { result: { result: { value } } } =
        await pProtocol.Runtime.evaluate({ expression: 'document.title' });
    return value;
  }

  testRunner.log(`No proxy page title: ${await dumpWithProxyServer()}`);
  testRunner.log(`Bogus proxy page title: ${await dumpWithProxyServer(
      'bogus')}`);
  testRunner.log(`Good proxy page title: ${await dumpWithProxyServer(
      new URL(testRunner._targetBaseURL).host)}`);

  testRunner.completeTest();
})

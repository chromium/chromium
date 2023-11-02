// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  testRunner.log('Tests that headless session can configure proxy.\n');
  const { result: { sessionId } } =
      await testRunner.browserP().Target.attachToBrowserTarget({});
  const { protocol: bProtocol } = new TestRunner.Session(testRunner, sessionId);

  async function dumpWithProxyServer(targetUrl, proxyServer) {
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
    await pProtocol.Page.navigate({ url: targetUrl });
    await pProtocol.Page.onceLoadEventFired();
    const { result: { result: { value } } } =
        await pProtocol.Runtime.evaluate(
            { expression: 'document.body.innerText' });
    return value;
  }

  testRunner.log(`No proxy: ${await dumpWithProxyServer(
      testRunner._testBaseURL + 'resources/body.html'
  )}`);

  testRunner.log(`Proxied to itself: ${await dumpWithProxyServer(
      testRunner._testBaseURL + 'resources/body.html',
      new URL(testRunner._targetBaseURL).host)}`);

  testRunner.log(`Proxied to another server: ${await dumpWithProxyServer(
      'http://not-an-actual-domain.tld/hello.html',
      testRunner.params('proxy'))}`);

  testRunner.completeTest();
})

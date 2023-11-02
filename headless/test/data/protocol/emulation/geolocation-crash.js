// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  testRunner.log('Tests geolocation request does not crash headless.\n');
  const { result: { sessionId } } =
      await testRunner.browserP().Target.attachToBrowserTarget({});
  const { protocol: bProtocol } = new TestRunner.Session(testRunner, sessionId);
  const { result: { browserContextId } } =
      await bProtocol.Target.createBrowserContext();
  await bProtocol.Browser.grantPermissions({
    browserContextId,
    permissions: ['geolocation']
  });

  {
    // Create page
    const { result: { targetId }} = await bProtocol.Target.createTarget({
      browserContextId,
      url: 'about:blank'
    });
    const { result: { sessionId } } =
        await bProtocol.Target.attachToTarget({ targetId, flatten: true });
    const { protocol: pProtocol } =
        new TestRunner.Session(testRunner, sessionId);

    // We need to load page off HTTPS, so use interception
    {
      const FetchHelper = await testRunner.loadScriptAbsolute(
        '../fetch/resources/fetch-test.js');
      const helper = new FetchHelper(testRunner, pProtocol);
      helper.onceRequest('https://test.com/index.html')
          .fulfill(FetchHelper.makeContentResponse(`<html></html>`));
      await helper.enable();
    }

    await pProtocol.Emulation.setGeolocationOverride({});
    await pProtocol.Page.navigate({ url: 'https://test.com/index.html' });

    // It does not matter if this returns error, we are only testing for
    // the crash.
    await pProtocol.Runtime.evaluate({
      expression: `new Promise(f => {
        navigator.geolocation.getCurrentPosition(
          pos => f("success"),
          e => f("error")
        );
      })`,
      awaitPromise: true,
    });
    testRunner.log('No crash');
  }

  testRunner.completeTest();
});

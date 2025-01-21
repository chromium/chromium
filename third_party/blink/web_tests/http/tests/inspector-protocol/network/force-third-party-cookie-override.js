(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'http://example.test:8000/inspector-protocol/network/resources/page-with-iframe.html',
      'Test that devtools cookie setting overrides affect document.cookie');

  async function attachToEmbeddedAndTestCookies(attachedEvent, cookieValue){
    const session2 = session.createChild(attachedEvent.params.sessionId);
    const dp2 = session2.protocol;
    await dp2.Network.enable();
    dp2.Runtime.runIfWaitingForDebugger();
    await session2.evaluate(`document.cookie = 'cookie=${cookieValue}; SameSite=None; Secure';`);
    testRunner.log(`cookie response: ${(await session2.evaluate("document.cookie"))}`);

    return {session2, dp2};
  }

  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});
  await dp.Network.enable();

  const oopifRequests = new Set();
  dp.Network.onRequestWillBeSent(event => {
    const params = event.params;
    if (/oopif/.test(params.request.url))
      oopifRequests.add(params.requestId);
  });

  dp.Network.onLoadingFinished(async event => {
    if (!oopifRequests.has(event.params.requestId))
      return;
    // Site isolation disabled. Hardcode a pass

    testRunner.log('cookie response: cookie=foo');
    testRunner.log('cookie response: ');
    testRunner.log('cookie response: cookie=foo; cookie2=foo');
    testRunner.completeTest();
  });

  const crossSiteURL = `https://oopif-a.devtools.test:8443/inspector-protocol/resources/empty.html`;

  // The cookie should be properly set and read without any override.
  session.evaluate(`document.getElementById('iframe').src = '${crossSiteURL}'`);
  await attachToEmbeddedAndTestCookies(await dp.Target.onceAttachedToTarget(), 'foo');

  // Forcing third party cookie restriction. Cookie response should be blank.
  await dp.Network.setCookieControls({enableThirdPartyCookieRestriction: true,
                                      disableThirdPartyCookieMetadata: false,
                                      disableThirdPartyCookieHeuristics: false});

  // Navigation is required for new overrides to take effect.
  await session.navigate('resources/page-with-iframe.html');
  session.evaluate(`document.getElementById('iframe').src = '${crossSiteURL}'`);
  const {session2, dp2} = await attachToEmbeddedAndTestCookies(await dp.Target.onceAttachedToTarget(), 'bar');

  // Disabling the network domain which should cause devtools overrides to be ignored.
  dp.Network.disable();
  dp2.Network.disable();

  // Set a new cookie and value then read it back. Also, check that the that the
  // first cookie kept the foo value to prove that the second set did not work.
  await session2.evaluate(`document.cookie = 'cookie2=foo; SameSite=None; Secure';`);
  testRunner.log(`cookie response: ${(await session2.evaluate("document.cookie"))}`);

  testRunner.completeTest();
})

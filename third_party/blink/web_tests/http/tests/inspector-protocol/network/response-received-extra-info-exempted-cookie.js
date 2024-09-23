(async testRunner => {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the cookie exemption reason is in responseReceivedExtraInfo.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await dp.Network.enable();

  // Enable for debugging purpose.
  // await dp.Runtime.enable();

  // Push events to arrays to prevent async races from causing flakes.
  const responseReceivedExtraInfos = [];
  let gotAllResponses = null;

  const onResponseReceivedExtraInfo = (event) => {
    responseReceivedExtraInfos.push(event.params);
    if (responseReceivedExtraInfos.length === 2) {
      gotAllResponses();
    }
  };
  const expectedResponses = new Promise(resolve => {gotAllResponses = resolve});

  dp.Network.onResponseReceivedExtraInfo(onResponseReceivedExtraInfo)

  dp.Target.onAttachedToTarget(async event => {
    const dp1 = session.createChild(event.params.sessionId).protocol;
    await dp1.Network.enable();
    // Enable for debugging purpose.
    // await dp1.Runtime.enable();
    testRunner.log('Iframe Network Enabled');
    dp1.Network.onResponseReceivedExtraInfo(onResponseReceivedExtraInfo);
    await dp1.Runtime.runIfWaitingForDebugger();
  });

  session.evaluate(function() {
    const frame = document.createElement('iframe');
    frame.src =
        'https://example.test:8443/inspector-protocol/resources/iframe-third-party-cookie-child-request-storage-access.html';
    new Promise(fulfill => frame.addEventListener('load', fulfill));
    document.body.appendChild(frame);
  });

  await expectedResponses;

  for (const params of responseReceivedExtraInfos) {
    testRunner.log(
        `Number of exempted cookies: ${params.exemptedCookies.length}`);
    for (const cookie of params.exemptedCookies) {
      testRunner.log(cookie);
    }
  }

  testRunner.completeTest();
})

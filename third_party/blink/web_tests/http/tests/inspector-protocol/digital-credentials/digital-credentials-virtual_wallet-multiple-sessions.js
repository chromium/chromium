(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session: session1, dp: dp1} = await testRunner.startBlank(
      'Check that virtual wallet behavior persists across multiple sessions and resets when there is no active session');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  await dp1.DigitalCredentials.setVirtualWalletBehavior({
    action: 'respond',
    protocol: 'openid4vp-v1-unsigned',
    response: {"token":"token1"},
  });

  // Session 1 should get the configured virtual wallet response first.
  const resultSession1 = await session1.evaluateAsyncWithUserGesture(`getDigitalCredential({
    protocol: 'openid4vp-v1-unsigned',
    data: {},
  })`);
  testRunner.log('Session1 result: ' + JSON.stringify(resultSession1));

  const session2 = await page.createSession();
  const resultSession2BeforeDisconnect = await session2.evaluateAsyncWithUserGesture(`getDigitalCredential({
    protocol: 'openid4vp-v1-unsigned',
    data: {},
  })`);
  testRunner.log('Session2 before disconnect: ' + JSON.stringify(resultSession2BeforeDisconnect));

  // Disconnect session 2 and verify session 1 still gets the wallet response.
  await session2.disconnect();

  const resultSession1AfterSession2Disconnect = await session1.evaluateAsyncWithUserGesture(`getDigitalCredential({
    protocol: 'openid4vp-v1-unsigned',
    data: {},
  })`);
  testRunner.log('Session1 after session2 disconnect: ' + JSON.stringify(resultSession1AfterSession2Disconnect));

  // Disconnect session 1, then confirm virtual wallet is reset.
  await session1.disconnect();

  const session3 = await page.createSession();
  const resultSession3 = await session3.evaluateAsyncWithUserGesture(`getDigitalCredential({
    protocol: 'openid4vp-v1-unsigned',
    data: {},
  })`);
  testRunner.log('Session3 after session1 disconnect: ' + JSON.stringify(resultSession3));

  testRunner.completeTest();
})

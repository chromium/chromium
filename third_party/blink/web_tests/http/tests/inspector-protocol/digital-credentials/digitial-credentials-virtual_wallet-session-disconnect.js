(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that virtual wallet state is released once the CDP session disconnects');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'respond',
    protocol: 'openid4vp-v1-unsigned',
    response: {"token":"session1-wallet-token"},
  });

  // Verify the virtual wallet responds while session is active.
  const resultBeforeDisconnect = await session.evaluateAsyncWithUserGesture(`getDigitalCredential({
        protocol: 'openid4vp-v1-unsigned',
        data: {},
      })`);

  // Disconnect the first session.
  await session.disconnect();

  const session2 = await page.createSession();
  // After disconnecting, the virtual wallet state set by session should be
  // released. Requests should no longer be intercepted by the virtual wallet.
  const resultAfterDisconnect = await session2.evaluateAsyncWithUserGesture(`getDigitalCredential({
        protocol: 'openid4vp-v1-unsigned',
        data: {},
      })`);

  testRunner.log(`First session: ${JSON.stringify(resultBeforeDisconnect)}`);
  testRunner.log(`Second session: ${JSON.stringify(resultAfterDisconnect)}`);
  testRunner.completeTest();
})

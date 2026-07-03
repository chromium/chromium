(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that virtual wallet is deleted on navigations');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'respond',
    protocol: 'openid4vp-v1-unsigned',
    response: {"token":"virtual-wallet-token"},
  });

  const beforeNavigationResult = await session.evaluateAsyncWithUserGesture(`getDigitalCredential({
        protocol: 'openid4vp-v1-unsigned',
        data: {},
      })`);

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  // Verify virtual wallet state is deleted
  const afterNavigationResult = await session.evaluateAsyncWithUserGesture(`getDigitalCredential({
        protocol: 'openid4vp-v1-unsigned',
        data: {},
      })`);

  await dp.DigitalCredentials.setVirtualWalletBehavior({action: 'clear'});

  testRunner.log('before navigation result:' + JSON.stringify(beforeNavigationResult));
  testRunner.log('after navigation result:' + JSON.stringify(afterNavigationResult));
  testRunner.completeTest();
})

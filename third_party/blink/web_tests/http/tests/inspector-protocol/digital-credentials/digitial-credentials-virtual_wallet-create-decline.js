(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that Digital Credentials API create call can be declined by virtual wallet');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-create.html');

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'decline',
  });

  const result = await session.evaluateAsyncWithUserGesture(`createDigitalCredential({
      protocol: 'openid4vci-v1',
        data: {},
      })`);

  await dp.DigitalCredentials.setVirtualWalletBehavior({action: 'clear'});

  testRunner.log(result);
  testRunner.completeTest();
})
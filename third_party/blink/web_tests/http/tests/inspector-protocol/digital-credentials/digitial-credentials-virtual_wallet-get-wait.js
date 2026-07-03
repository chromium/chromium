(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that Digital Credentials API call can be aborted when virtual wallet waits');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'wait',
  });

  const result = await session.evaluateAsyncWithUserGesture(`(async () => {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 100);

    try {
      return await getDigitalCredential({
        protocol: 'openid4vp-v1-unsigned',
        data: {},
      }, controller.signal);
    } finally {
      clearTimeout(timer);
    }
  })()`);

  await dp.DigitalCredentials.setVirtualWalletBehavior({action: 'clear'});

  testRunner.log(result);
  testRunner.completeTest();
})

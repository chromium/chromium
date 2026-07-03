(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that Digital Credentials API create call can be aborted when virtual wallet waits');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-create.html');

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'wait',
  });

  const result = await session.evaluateAsyncWithUserGesture(`(async () => {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 100);

    try {
      return await createDigitalCredential({
        protocol: 'openid4vci-v1',
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
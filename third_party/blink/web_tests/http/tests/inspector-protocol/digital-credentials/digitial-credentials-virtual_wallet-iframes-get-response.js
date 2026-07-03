(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that iframe can use its parent frame virtual wallet response');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'respond',
    protocol: 'openid4vp-v1-unsigned',
    response: {"token":"virtual-wallet-main-frame"},
  });

  // Get credential from main page
  const mainPageResult = await session.evaluateAsyncWithUserGesture(`getDigitalCredential({
        protocol: 'openid4vp-v1-unsigned',
        data: {},
      })`);

  // Create an iframe first.
  await session.evaluateAsync(`
    new Promise((resolve) => {
      const iframe = document.createElement('iframe');
      iframe.id = 'digital-credentials-iframe';
      iframe.allow = 'digital-credentials-get';
      iframe.src = 'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html';
      iframe.onload = () => resolve();
      document.body.appendChild(iframe);
    })
  `);

  // Invoke credentials.get() in a separate user gesture to preserve transient activation.
  const iframeResult = await session.evaluateAsyncWithUserGesture(`
    (async () => {
      const iframe = document.getElementById('digital-credentials-iframe');
      try {
        const credentials = await iframe.contentWindow.navigator.credentials.get({
          digital: {
            requests: [{
              protocol: 'openid4vp-v1-unsigned',
              data: {},
            }],
          },
        });

        const response = credentials ? credentials.toJSON() : null;
        return {response, error: null};
      } catch (error) {
        const errorName = error && error.name ? error.name : String(error);
        return {response: null, error: errorName};
      }
    })()
  `);

  await dp.DigitalCredentials.setVirtualWalletBehavior({action: 'clear'});

  testRunner.log('main page:' + JSON.stringify(mainPageResult));
  testRunner.log('iframe:' + JSON.stringify(iframeResult));
  testRunner.completeTest();
})

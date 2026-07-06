(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that virtual wallet behavior scoped to an iframe does not leak to other frames in the same target');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html');

  await dp.Page.enable();

  // Create two iframes in the main document.
  await session.evaluateAsync(`
    new Promise((resolve) => {
      let loaded = 0;
      function checkLoaded() {
        if (++loaded === 2) resolve();
      }
      const iframe1 = document.createElement('iframe');
      iframe1.id = 'iframe1';
      iframe1.allow = 'digital-credentials-get';
      iframe1.src = 'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html';
      iframe1.onload = checkLoaded;
      document.body.appendChild(iframe1);

      const iframe2 = document.createElement('iframe');
      iframe2.id = 'iframe2';
      iframe2.allow = 'digital-credentials-get';
      iframe2.src = 'https://devtools.test:8443/inspector-protocol/digital-credentials/resources/digital-credentials-get.html';
      iframe2.onload = checkLoaded;
      document.body.appendChild(iframe2);
    })
  `);

  // Retrieve frame IDs from the frame tree.
  const {result: {frameTree}} = await dp.Page.getFrameTree();
  const iframe1Id = frameTree.childFrames[0].frame.id;

  // Scope virtual wallet behavior specifically to iframe1.
  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'respond',
    protocol: 'openid4vp-v1-unsigned',
    response: {"token":"scoped-iframe-token"},
    frameId: iframe1Id,
  });

  // 1. Query within iframe1: should succeed with the scoped virtual wallet response.
  const iframe1Result = await session.evaluateAsyncWithUserGesture(`
    (async () => {
      const iframe = document.getElementById('iframe1');
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
        return {response: null, error: error && error.name ? error.name : String(error)};
      }
    })()
  `);

  // 2. Query within iframe2: should NOT get the scoped response (returns fake UI default or error).
  const iframe2Result = await session.evaluateAsyncWithUserGesture(`
    (async () => {
      const iframe = document.getElementById('iframe2');
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
        return {response: null, error: error && error.name ? error.name : String(error)};
      }
    })()
  `);

  // 3. Query within main frame: should NOT get the scoped response.
  const mainFrameResult = await session.evaluateAsyncWithUserGesture(`getDigitalCredential({
    protocol: 'openid4vp-v1-unsigned',
    data: {},
  })`);

  await dp.DigitalCredentials.setVirtualWalletBehavior({
    action: 'clear',
    frameId: iframe1Id,
  });

  testRunner.log('iframe1 result: ' + JSON.stringify(iframe1Result));
  testRunner.log('iframe2 result: ' + JSON.stringify(iframe2Result));
  testRunner.log('main frame result: ' + JSON.stringify(mainFrameResult));
  testRunner.completeTest();
})

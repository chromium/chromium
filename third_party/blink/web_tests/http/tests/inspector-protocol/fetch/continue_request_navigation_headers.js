(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests setting headers via Fetch.continueRequest for navigation requests.`);

  await dp.Fetch.enable();
  await dp.Network.enable();
  // Verify that the headers are set via Network.setExtraHTTPHeaders are
  // overridden by Fetch.continueRequest.
  await dp.Network.setExtraHTTPHeaders({
    headers: {
      'referer': 'http://example.com',
      'X-Devtools-Test': 'bar',
      'X-Devtools-Test-2': 'qux'
    }
  });

  async function get_navigation_headers(url) {
    return await session.evaluateAsync(`(async () => {
      const iframe = document.createElement('iframe');
      iframe.src = '${url}';
      document.body.appendChild(iframe);
      return new Promise(resolve => {
        iframe.addEventListener('load', () => {
          resolve(iframe.contentWindow.document.body.innerText.trim());
          iframe.remove();
        });
      });
    })();`);
  }

  const headers_promise = get_navigation_headers(testRunner.url(
      '../network/resources/echo-headers.php?headers=HTTP_X_DEVTOOLS_TEST:HTTP_X_DEVTOOLS_TEST_2:HTTP_COOKIE:HTTP_REFERER'));

  const pausedRequest = (await dp.Fetch.onceRequestPaused()).params;
  dp.Fetch.continueRequest({
    requestId: pausedRequest.requestId,
    headers: [
      {name: 'X-Devtools-Test', value: 'foo'},
      {name: 'Cookie', value: 'bar=bazz'},
      {name: 'Referer', value: 'http://google.com'},
    ]
  });

  testRunner.log(await headers_promise);
  testRunner.completeTest();
})

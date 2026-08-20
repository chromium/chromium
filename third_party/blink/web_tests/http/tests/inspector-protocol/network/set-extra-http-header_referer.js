(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests setting referer via Network.setExtraHTTPHeaders.`);

  await dp.Network.enable();

  // Verify requestWillBeSent does not report the referer twice under
  // different casings.
  const reportedRefererEntries = [];
  dp.Network.onRequestWillBeSent(event => {
    if (event.params.type !== 'Document')
      return;
    const headers = event.params.request.headers;
    reportedRefererEntries.push(
        Object.keys(headers)
            .filter(name => name.toLowerCase() === 'referer')
            .map(name => `${name}: ${headers[name]}`));
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

  async function log_navigation(label, url) {
    testRunner.log(label);
    testRunner.log(await get_navigation_headers(url));
    testRunner.log(`requestWillBeSent referer entries: ${
        JSON.stringify(reportedRefererEntries.splice(0))}`);
  }

  // The url returns the HTTP_REFERER header in the response body.
  const test_url =
      testRunner.url('./resources/echo-headers.php?headers=HTTP_REFERER');
  // The url redirects to the echo-headers.php url, which returns the
  // HTTP_REFERER header in the response body.
  const redirect_url =
      testRunner.url(`../fetch/resources/redirect.pl?${test_url}`);

  const testCases = [
    // Happy case.
    {'referer': 'http://google.com'},
    // Different casing for the header name.
    {'rEfErEr': 'http://google.com'},
    // Invalid referer URL string.
    {'referer': 'some invalid url'},
    // Invalid referer value type (number instead of string).
    {'referer': 1},
    // Missing referer header.
    {},
  ];

  for (const testCase of testCases) {
    await dp.Network.setExtraHTTPHeaders({headers: testCase});

    testRunner.log(`\nTest case: ${JSON.stringify(testCase)}:`);
    await log_navigation('Direct navigation:', test_url);
    await log_navigation('Redirect:', redirect_url);
  }

  testRunner.completeTest();
})

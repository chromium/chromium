(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests setting referer via Network.setExtraHTTPHeaders.`);

  await dp.Network.enable();

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
    testRunner.log('Direct navigation:');
    testRunner.log(await get_navigation_headers(test_url));
    testRunner.log('Redirect:');
    testRunner.log(await get_navigation_headers(redirect_url));
  }

  testRunner.completeTest();
})

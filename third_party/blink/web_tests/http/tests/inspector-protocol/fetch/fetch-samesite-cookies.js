(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that fetch exposes cookies according to SameSite rules.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  await helper.enable();

  helper.onceRequest(/setcookies.a.test/).fulfill({
    responseCode: 200,
    responseHeaders: [
      {name: 'Set-Cookie', value: 'StrictCookie=1; Secure; SameSite=Strict; Domain=a.test'},
      {name: 'Set-Cookie', value: 'LaxCookie=1; Secure; SameSite=Lax; Domain=a.test'},
      {name: 'Set-Cookie', value: 'NoneCookie=1; Secure; SameSite=None; Domain=a.test'},
      {name: 'Set-Cookie', value: 'UnspecifiedCookie=1; Secure; Domain=a.test'}
    ],
    body: btoa("<html></html>")
  });

  await dp.Page.enable();
  await session.navigate('https://setcookies.a.test/');

  // URL whose cookies are dumped in the following tests.
  const cookieUrl = 'https://a.test/';

  // Set up redirect to a.test from any URL containing 'redirect'.
  helper.onRequest(/redirect/).fulfill({
    responseCode: 302,
    responseHeaders: [{name: 'Location', value: cookieUrl}]
  });

  // Returns a string containing a script to append an iframe to the DOM.
  function appendIframeScript(url) {
    // This may run before or after the page is fully loaded, so account for
    // both cases to avoid flakiness.
    return `
        function appendIframe() {
          var frame = document.createElement('iframe');
          frame.src = '${url}';
          document.body.appendChild(frame);
        }
        window.onload = appendIframe;
        if (document.readyState === 'complete')
          appendIframe();`
  }

  // Navigates to `fromUrl`, then navigates to a.test and dumps cookies that
  // were sent.
  async function navigateAndDumpCookies(fromUrl, description) {
    // Navigate to the starting location.
    helper.onceRequest(new RegExp(fromUrl)).fulfill({
      responseCode: 200,
      body: btoa("<html></html>")
    });
    await dp.Page.navigate({url: fromUrl});
    // Navigate to a.test.
    session.evaluate(`location.href = '${cookieUrl}'`);
    const request = await helper.onceRequest(cookieUrl).matched();
    testRunner.log(`Cookies after ${description}:`);
    testRunner.log(request.request.headers['Cookie']);
    dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});
  }

  // Navigates to `mainUrl`, then loads a.test in an iframe and dumps cookies
  // that were sent.
  async function loadIframeAndDumpCookies(mainUrl, description) {
    // Navigate to the main page URL.
    helper.onceRequest(new RegExp(mainUrl)).fulfill({
      responseCode: 200,
      body: btoa("<html></html>")
    });
    await dp.Page.navigate({url: mainUrl});
    // Load iframe with a.test.
    session.evaluate(appendIframeScript(cookieUrl));
    const request = await helper.onceRequest(cookieUrl).matched();
    testRunner.log(`Cookies for ${description}:`);
    testRunner.log(request.request.headers['Cookie']);
    dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});
  }

  // Navigates to `fromUrl`, then navigates to `redirectUrl`, which then
  // redirects to a.test and dumps cookies that were sent.
  // `redirectUrl` must match the pattern /redirect/.
  async function redirectAndDumpCookies(fromUrl, redirectUrl, description) {
    helper.onceRequest(new RegExp(fromUrl)).fulfill({
      responseCode: 200,
      body: btoa("<html></html>")
    });
    // Navigate to the starting location.
    await dp.Page.navigate({url: fromUrl});
    // Navigate to `redirectUrl` which then redirects to a.test.
    session.evaluate(`location.href = '${redirectUrl}'`);
    const request = await helper.onceRequest(cookieUrl).matched();
    testRunner.log(`Cookies for ${description}:`);
    testRunner.log(request.request.headers['Cookie']);
    dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});
  }

  // Navigates to `mainUrl`, then loads `redirectUrl` in an iframe. The iframe
  // then redirects to a.test and dumps cookies that were sent.
  // `redirectUrl` must match the pattern /redirect/.
  async function redirectInIframeAndDumpCookies(mainUrl, redirectUrl, description) {
    helper.onceRequest(new RegExp(mainUrl)).fulfill({
      responseCode: 200,
      body: btoa("<html></html>")
    });
    // Navigate to the main page URL.
    await dp.Page.navigate({url: mainUrl});
    // Load an iframe with `redirectUrl` which then redirects to a.test.
    session.evaluate(appendIframeScript(redirectUrl));
    const request = await helper.onceRequest(cookieUrl).matched();
    testRunner.log(`Cookies for ${description}:`);
    testRunner.log(request.request.headers['Cookie']);
    dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});
  }

  await navigateAndDumpCookies('https://subdomain.a.test/', 'same-site navigation');
  await navigateAndDumpCookies('https://b.test/', 'cross-site navigation');
  await loadIframeAndDumpCookies('https://subdomain.a.test/', 'same-site iframe');
  await loadIframeAndDumpCookies('https://b.test/', 'cross-site iframe');
  await redirectAndDumpCookies('https://subdomain.a.test/', 'https://redirect.a.test/', 'same-site initiated same-site redirect');
  await redirectAndDumpCookies('https://subdomain.a.test/', 'https://redirect.b.test/', 'same-site initiated cross-site redirect');
  await redirectAndDumpCookies('https://b.test/', 'https://redirect.a.test/', 'cross-site initiated same-site redirect');
  await redirectAndDumpCookies('https://b.test/', 'https://redirect.b.test/', 'cross-site initiated cross-site redirect');
  await redirectInIframeAndDumpCookies('https://subdomain.a.test/', 'https://redirect.a.test/', 'same-site embedded same-site redirect');
  await redirectInIframeAndDumpCookies('https://subdomain.a.test/', 'https://redirect.b.test/', 'same-site embedded cross-site redirect');
  await redirectInIframeAndDumpCookies('https://b.test/', 'https://redirect.a.test/', 'cross-site embedded same-site redirect');
  await redirectInIframeAndDumpCookies('https://b.test/', 'https://redirect.b.test/', 'cross-site embedded cross-site redirect');

  testRunner.completeTest();
})

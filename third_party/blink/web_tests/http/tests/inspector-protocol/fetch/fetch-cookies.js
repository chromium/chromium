(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that fetch exposes cookies for CORS XHRs.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  await helper.enable();

  // site1.a.test and site2.a.test are cross-origin but same-site.
  helper.onceRequest(/site1.a.test/).fulfill({
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: 'https://site2.a.test'},
      {name: 'Set-Cookie', value: 'CrossOriginCookie=om-nom-nom-nom; path=/; domain=.a.test; Secure; HttpOnly; SameSite=none'},
    ]
  });

  helper.onceRequest(/site2.a.test/).fulfill({
    responseCode: 200,
    responseHeaders: [
        {name: 'Set-Cookie', value: 'SameOriginCookie=me-want-cookie; Secure; domain=site2.a.test; HttpOnly; SameSite=none'},
    ],
    body: btoa("<html></html>")
  });

  // b.test is both cross-origin and cross-site to site2.a.test.
  helper.onceRequest(/b.test/).fulfill({
    responseCode: 200,
    responseHeaders: [
        {name: 'Set-Cookie', value: 'CrossSiteCookie=c-is-for-cookie; Secure; domain=b.test; HttpOnly; SameSite=none'},
    ],
    body: btoa("<html></html>")
  });


  await dp.Page.enable();
  // Navigate once to b.test just to set cookies.
  await session.navigate('https://b.test');
  // The rest of the test happens in the context of https://site2.a.test.
  await session.navigate('https://site1.a.test');

  async function makeRequestAndDumpCookies(pattern, code, description) {
    session.evaluate(code);
    const request = await helper.onceRequest(pattern).matched();
    testRunner.log(`Cookies after ${description}:`);
    testRunner.log(request.request.headers['Cookie']);
    dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});
  }

  await makeRequestAndDumpCookies(/a.test/, `
      const xhr = new XMLHttpRequest();
      xhr.open('POST', 'https://site1.a.test/post');
      xhr.withCredentials = true;
      xhr.send('postdata');
      `, 'cross-origin (same-site) XHR');

  await makeRequestAndDumpCookies(/a.test/, `
      const xhr2 = new XMLHttpRequest();
      xhr2.open('POST', '/post');
      xhr2.send('postdata');
      `, 'same-origin XHR');

  await makeRequestAndDumpCookies(/a.test/, `
      fetch('https://site1.a.test/post',
          {method: 'POST', body: 'postdata', credentials: 'include'});
      `, `cross-origin (same-site) fetch with {credentials: 'include'}`);

  await makeRequestAndDumpCookies(/a.test/, `
      fetch('https://site1.a.test/post',
          {method: 'POST', body: 'postdata', credentials: 'same-origin'});
      `, `cross-origin (same-site) fetch with {credentials: 'same-origin'}`);

  await makeRequestAndDumpCookies(/a.test/, `
      fetch('/post',
          {method: 'POST', body: 'postdata', credentials: 'same-origin'});
      `, `same-origin fetch with {credentials: 'same-origin'}`);

  await makeRequestAndDumpCookies(/a.test/, `
      fetch('/post',
          {method: 'POST', body: 'postdata', credentials: 'include', mode: 'no-cors'});
      `, `same-origin fetch with {credentials: 'include', mode: 'no-cors'}`);

  await makeRequestAndDumpCookies(/a.test/, `
      fetch('https://site1.a.test/post',
          {method: 'POST', body: 'postdata', credentials: 'include', mode: 'no-cors'});
      `, `cross-origin (same-site) fetch with {credentials: 'include', mode: 'no-cors'}`);

  await makeRequestAndDumpCookies(/b.test/, `
      const xhr3 = new XMLHttpRequest();
      xhr3.open('POST', 'https://b.test/post');
      xhr3.withCredentials = true;
      xhr3.send('postdata');
      `, 'cross-origin (cross-site) XHR');

  await makeRequestAndDumpCookies(/b.test/, `
      fetch('https://b.test/post',
          {method: 'POST', body: 'postdata', credentials: 'include'});
      `, `cross-origin (cross-site) fetch with {credentials: 'include'}`);

  await makeRequestAndDumpCookies(/b.test/, `
      fetch('https://b.test/post',
          {method: 'POST', body: 'postdata', credentials: 'same-origin'});
      `, `cross-origin (cross-site) fetch with {credentials: 'same-origin'}`);

  await makeRequestAndDumpCookies(/b.test/, `
      fetch('https://b.test/post',
          {method: 'POST', body: 'postdata', credentials: 'include', mode: 'no-cors'});
      `, `cross-origin (cross-site) fetch with {credentials: 'include', mode: 'no-cors'}`);

  testRunner.completeTest();
})


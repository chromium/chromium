(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that fetch exposes cookies for CORS XHRs.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  await helper.enable();

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

  await dp.Page.enable();
  await session.navigate('https://site1.a.test');

  async function makeRequestAndDumpCookies(code, description) {
    session.evaluate(code);
    const request = await helper.onceRequest(/a.test/).matched();
    testRunner.log(`Cookies after ${description}:`);
    testRunner.log(request.request.headers['Cookie']);
    dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});
  }

  await makeRequestAndDumpCookies(`
      const xhr = new XMLHttpRequest();
      xhr.open('POST', 'https://site1.a.test/post');
      xhr.send('postdata');
      `, 'cross-origin XHR');

  await makeRequestAndDumpCookies(`
      const xhr2 = new XMLHttpRequest();
      xhr2.open('POST', '/post');
      xhr2.send('postdata');
      `, 'same-origin XHR');

  await makeRequestAndDumpCookies(`
      fetch('https://site1.a.test/post',
          {method: 'POST', body: 'postdata', credentials: 'include'});
      `, `cross-origin fetch with {credentials: 'include'}`);

  await makeRequestAndDumpCookies(`
      fetch('https://site1.a.test/post',
          {method: 'POST', body: 'postdata', credentials: 'same-origin'});
      `, `cross-origin fetch with {credentials: 'same-origin'}`);

  await makeRequestAndDumpCookies(`
      fetch('/post',
          {method: 'POST', body: 'postdata', credentials: 'same-origin'});
      `, `same-origin fetch with {credentials: 'same-origin'}`);

  await makeRequestAndDumpCookies(`
      fetch('/post',
          {method: 'POST', body: 'postdata', credentials: 'include', mode: 'no-cors'});
      `, `same-origin fetch with {credentials: 'include', mode: 'no-cors'}`);

  await makeRequestAndDumpCookies(`
      fetch('https://site1.a.test/post',
          {method: 'POST', body: 'postdata', credentials: 'include', mode: 'no-cors'});
      `, `cross-origin fetch with {credentials: 'include', mode: 'no-cors'}`);

  testRunner.completeTest();
})


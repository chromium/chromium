(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that requests produced by redirects injected via mocked response are intercepted when followed.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await session.protocol.Network.enable();
  await session.protocol.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({enabled: true});

  await dp.Network.setRequestInterception({patterns: [{}]});

  const loadPromise = dp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  dp.Page.navigate({ url: 'http://test-url/' });

  let params = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted: ${params.request.url}`);
  respondWithRedirct(params, 'http://test-url/redirect1');

  params = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted: ${params.request.url}`);
  respondWithRedirct(params, 'http://test-url/redirect2');

  params = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted: ${params.request.url}`);
  respondWithRedirct(params, 'http://test-url/final');

  params = (await dp.Network.onceRequestIntercepted()).params;
  testRunner.log(`Intercepted: ${params.request.url}`);
  respond(params, ['HTTP/1.1 200 OK', 'Content-Type: text/html'], '<body>Hello, world!</body>');

  await loadPromise;
  const body = await session.evaluate('document.body.textContent');
  testRunner.log(`Response body: ${body}`);

  testRunner.completeTest();

  function respond(params, headers, body) {
    const headersText = headers.join("\r\n");
    const response = headersText + "\r\n\r\n" + (body || "");
    dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId, rawResponse: btoa(response)});
  }

  function respondWithRedirct(params, url) {
    testRunner.log(`Redirecting to ${url}`);
    respond(params, ['HTTP/1.1 302 Moved', `Location: ${url}`], null);
  }
})


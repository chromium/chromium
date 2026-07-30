(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that interception works with multiple clients.');

  const dp2 = (await page.createSession()).protocol;

  await dp.Network.clearBrowserCache();
  await dp.Network.clearBrowserCookies();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.enable();
  await dp.Runtime.enable();

  await dp2.Network.enable();
  testRunner.log('-- request stage for client 1 and client 2');
  await dp2.Fetch.enable({patterns: [{}]});
  await dp.Fetch.enable({patterns: [{}]});

  function continueInterceptedRequest(protocol, clientName, event) {
    const params = event.params;
    const url = params.request.url;
    const is_response = !!params.responseHeaders;

    if (is_response) {
      testRunner.log(`${clientName}: intercepted response ${
          params.responseStatusCode} from ${url}`);
    } else {
      testRunner.log(`${clientName}: intercepted request to ${url}`);
    }
    protocol.Fetch.continueRequest({requestId: params.requestId});
  }

  const listener1 = continueInterceptedRequest.bind(this, dp, 'client 1');
  dp.Fetch.onRequestPaused(listener1);
  dp2.Fetch.onRequestPaused(
      continueInterceptedRequest.bind(this, dp2, 'client 2'));

  await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- request stage for client 1, both stages for client 2');

  await dp2.Fetch.enable({
    patterns: [
      {urlPattern: '*', requestStage: 'Request'},
      {urlPattern: '*', requestStage: 'Response'}
    ]
  });

  await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- both stages for client 1 and client 2');

  await dp.Fetch.enable({
    patterns: [
      {urlPattern: '*', requestStage: 'Request'},
      {urlPattern: '*', requestStage: 'Response'}
    ]
  });

  await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- mock response from client 1');

  dp.Fetch.offRequestPaused(listener1);
  dp.Fetch.onRequestPaused(event => {
    const params = event.params;
    testRunner.log(`client 1: rejecting request to ${params.request.url}`);
    dp.Fetch.fulfillRequest({
      requestId: params.requestId,
      responseCode: 418,
      responsePhrase: 'I\'m a teapot'
    });
  });

  await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- mock response from client 3');

  const dp3 = (await page.createSession()).protocol;
  await dp3.Fetch.enable({patterns: [{}]});
  dp3.Fetch.onceRequestPaused().then(event => {
    const params = event.params;
    testRunner.log(`client 3: resolving response to ${params.request.url}`);
    dp3.Fetch.fulfillRequest({
      requestId: params.requestId,
      responseCode: 200,
      responsePhrase: 'OK',
      body: btoa('Hello, world!')
    });
  });

  const body = await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);
  testRunner.log(`response: ${body}`);

  testRunner.log('-- url rewrite from client 3');

  dp3.Fetch.onceRequestPaused().then(event => {
    const params = event.params;
    const newURL = `${params.request.url}?jscontent=1`;
    testRunner.log(
        `client 3: overriding URL from ${params.request.url} to ${newURL}`);
    dp3.Fetch.continueRequest({requestId: params.requestId, url: newURL});
  });

  await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- failing request from client 3');

  dp3.Fetch.onceRequestPaused().then(event => {
    const params = event.params;
    testRunner.log(`client 3: failing request from ${params.request.url}`);
    dp3.Fetch.failRequest(
        {requestId: params.requestId, errorReason: 'Aborted'});
  });

  await session.evaluateAsync(
      `fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.completeTest();
})

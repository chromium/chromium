(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that interception works with multiple clients.');

  const dp2 = (await page.createSession()).protocol;

  await dp.Network.clearBrowserCache();
  await dp.Network.clearBrowserCookies();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.enable();
  await dp.Runtime.enable();

  await dp2.Network.enable();
  testRunner.log('-- request stage for client 1 and client 2');
  await dp2.Network.setRequestInterception({patterns: [{}]});
  await dp.Network.setRequestInterception({patterns: [{}]});

  function continueInterceptedRequest(protocol, clientName, event) {
    const params = event.params;
    const url = params.request.url;
    const is_response = !!params.responseHeaders;

    if (is_response) {
      testRunner.log(`${clientName}: intercepted response ${params.responseStatusCode} from ${url}`);
    } else {
      testRunner.log(`${clientName}: intercepted request to ${url}`);
    }
    protocol.Network.continueInterceptedRequest({interceptionId : params.interceptionId});
  }

  const listener1 = continueInterceptedRequest.bind(this, dp, "client 1");
  dp.Network.onRequestIntercepted(listener1);
  dp2.Network.onRequestIntercepted(continueInterceptedRequest.bind(this, dp2, "client 2"));

  await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- request stage for client 1, both stages for client 2');

  await dp2.Network.setRequestInterception({patterns: [
    {urlPattern: '*', interceptionStage: 'Request'},
    {urlPattern: '*', interceptionStage: 'HeadersReceived'}
  ]});

  await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- both stages for client 1 and client 2');

  await dp.Network.setRequestInterception({patterns: [
    {urlPattern: '*', interceptionStage: 'Request'},
    {urlPattern: '*', interceptionStage: 'HeadersReceived'}
  ]});

  await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- mock response from client 1');

  dp.Network.offRequestIntercepted(listener1);
  dp.Network.onRequestIntercepted(event => {
    const params = event.params;
    testRunner.log(`client 1: rejecting request to ${params.request.url}`);
    dp.Network.continueInterceptedRequest({interceptionId: params.interceptionId, rawResponse: btoa("HTTP/1.1 418 I'm a teapot\r\n\r\n")});
  });

  await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- mock response from client 3');

  const dp3 = (await page.createSession()).protocol;
  await dp3.Network.setRequestInterception({patterns: [{}]});
  dp3.Network.onceRequestIntercepted().then(event => {
    const params = event.params;
    testRunner.log(`client 3: resolving response to ${params.request.url}`);
    dp3.Network.continueInterceptedRequest({interceptionId: params.interceptionId, rawResponse: btoa("HTTP/1.1 200\r\n\r\nHello, world!")});
  });

  const body = await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);
  testRunner.log(`response: ${body}`);

  testRunner.log('-- url rewrite from client 3');

  dp3.Network.onceRequestIntercepted().then(event => {
    const params = event.params;
    const newURL = `${params.request.url}?jscontent=1`;
    testRunner.log(`client 3: overriding URL from ${params.request.url} to ${newURL}`);
    dp3.Network.continueInterceptedRequest({interceptionId: params.interceptionId, url: newURL});
  });

  await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.log('-- failing request from client 3');

  dp3.Network.onceRequestIntercepted().then(event => {
    const params = event.params;
    testRunner.log(`client 3: failing request from ${params.request.url}`);
    dp3.Network.continueInterceptedRequest({interceptionId: params.interceptionId, errorReason: "Aborted"});
  });

  await session.evaluateAsync(`fetch("/devtools/network/resources/resource.php").then(r => r.text())`);

  testRunner.completeTest();
})

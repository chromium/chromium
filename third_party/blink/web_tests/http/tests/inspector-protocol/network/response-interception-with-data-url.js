(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests to ensure iframe can change to data url while intercepting response.`);

  session.protocol.Network.onRequestIntercepted(async event => {
    var urlPart = event.params.request.url.split('/').pop();
    testRunner.log('Request Intercepted: ' + urlPart);

    // This will cause browser to cancel the request.
    if (!urlPart.startsWith('data:,')) {
      testRunner.log('Setting iframe to data url from renderer');
      await session.evaluate(`iframe.src ='data:,Dummy data';`);
    }
    testRunner.log('Continuing request unchanged');
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
    testRunner.log('');
  });

  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "http://*", interceptionStage: 'HeadersReceived'}]});

  var requestId = '';
  session.protocol.Network.onRequestWillBeSent(event => {
    if (!event.params.documentURL.startsWith('data:'))
      return;
    if (requestId)
      throw "requestId already set";
    requestId = event.params.requestId;
  });
  await new Promise(resolve => {
    session.protocol.Network.onLoadingFinished(resolve);
    session.evaluate(`
      iframe = document.createElement('iframe');
      // Script wait sends headers then waits 10 seconds to send body.
      iframe.src = '/devtools/network/resources/resource.php?send=10000&chunked=1&size=1000&nosniff=1';
      document.body.appendChild(iframe);
    `);
  });

  var result = await session.protocol.Network.getResponseBody({requestId: requestId});
  testRunner.log('Body content received by renderer:');
  testRunner.log(result.result.base64Encoded ? atob(result.result.body) : result.result.body);

  testRunner.completeTest();
})

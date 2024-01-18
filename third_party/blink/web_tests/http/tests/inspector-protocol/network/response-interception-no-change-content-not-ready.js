(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception to ensure if subrequest's content is not ready and we continue the renderer does not crash see: crbug.com/785502.`);

  session.protocol.Network.onRequestIntercepted(async event => {
    testRunner.log('Request Intercepted: ' + event.params.request.url.split('/').pop());
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
    testRunner.log('');
  });

  await session.protocol.Network.clearBrowserCookies();
  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  session.protocol.Network.enable();
  testRunner.log('Network agent enabled');
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*", interceptionStage: 'HeadersReceived'}]});

  var requestId = '';
  session.protocol.Network.onRequestWillBeSent(event => {
    if (requestId)
      throw "requestId already set";
    requestId = event.params.requestId;
  });
  await new Promise(resolve => {
    session.protocol.Network.onResponseReceived(resolve);
    session.evaluate(`
      window.xhr = new XMLHttpRequest();
      // Sends headers then waits .3 second to send body.
      window.xhr.open('GET', '/devtools/network/resources/resource.php?send=300', true);
      window.xhr.send();
    `);
  });

  var result = await session.protocol.Network.getResponseBody({requestId: requestId});
  testRunner.log('Body content received by renderer:');
  testRunner.log(result.result.base64Encoded ? atob(result.result.body) : result.result.body);

  testRunner.completeTest();
})

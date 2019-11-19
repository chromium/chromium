(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception for redirects in a chain but rewrite last response.`);

  var headersMaskList = new Set(['date', 'server', 'last-modified', 'etag', 'keep-alive', 'expires']);
  // Hide these headers which are not shown in newer versions of PHP.
  var headersHideList = new Set(['x-powered-by']);
  var fileNameForRequestId = new Map();
  var responseReceivedEventProimseForFile = new Map();
  var responseReceivedResolverForFile = new Map();

  await setUpInterception();
  testRunner.log('Test Ready.');

  await session.protocol.Network.setRequestInterception({patterns: [
    {urlPattern: '*', interceptionStage: 'Request'},
    {urlPattern: '*', interceptionStage: 'HeadersReceived'}
  ]});
  testRunner.log('Request interception patterns sent.');

  session.evaluate(`fetch('${testRunner.url('../resources/redirect1.php')}').then(r => r.text())`);

  await waitForInterceptionEventAndContinue("/redirect1.php");
  await waitForInterceptionEventAndContinue("/redirect1.php");


  // Should be redirect2.php as redirecting to final.html.
  await waitForInterceptionEventAndContinue("/redirect2.php");
  await waitForInterceptionEventAndContinue("/redirect2.php");

  // Should be final.html as request.
  await waitForInterceptionEventAndContinue("/final.html");
  const interceptionEvent = await waitForInterceptionEvent("/final.html");
  testRunner.log('Modifying final.html\'s response after we receive response.');
  var body = '<html>\n<body>This content was rewritten!</body>\n</html>';
  var dummyHeaders = [
    'HTTP/1.1 200 OK',
    'Date: ' + (new Date()).toUTCString(),
    'Connection: closed',
    'Content-Length: ' + body.size,
    'Content-Type: text/html'
  ];
  testRunner.log('Modifying request with new body.');
  session.protocol.Network.continueInterceptedRequest({
    interceptionId: interceptionEvent.params.interceptionId,
    rawResponse: btoa(dummyHeaders.join('\r\n') + '\r\n\r\n' + body)
  });

  var responseReceivedEvent = await waitForResponseReceivedEvent('final.html');
  var bodyResponse = await session.protocol.Network.getResponseBody({requestId: responseReceivedEvent.params.requestId});
  testRunner.log('');
  testRunner.log('Body content received by renderer for final.html:');
  testRunner.log(bodyResponse.result.base64Encoded ? atob(bodyResponse.result.body) : bodyResponse.result.body);
  testRunner.log('');
  testRunner.completeTest();


  async function setUpInterception() {
    await session.protocol.Network.clearBrowserCookies();
    await session.protocol.Network.clearBrowserCache();
    await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
    await session.protocol.Network.enable();
    await session.protocol.Page.enable();
    await session.protocol.Runtime.enable();
    session.protocol.Network.onRequestWillBeSent(event => {
      fileNameForRequestId.set(event.params.requestId, event.params.request.url.split('/').pop());
    });
    session.protocol.Network.onResponseReceived(event => {
      var fileName = fileNameForRequestId.get(event.params.requestId);
      if (!fileName)
        throw "Expected requestWillBeSent to be executed before responseReceived.";
      var resolver = responseReceivedResolverForFile.get(fileName);
      if (resolver) {
        resolver(event);
        responseReceivedResolverForFile.delete(fileName);
      }
      responseReceivedEventProimseForFile.set(fileName, Promise.resolve(event));
    });
  }

  async function waitForInterceptionEvent(expectedUrlSuffix) {
    const event = await session.protocol.Network.onceRequestIntercepted();
    const url = event.params.request.url;
    if (!url.endsWith(expectedUrlSuffix)) {
      testRunner.log(`FAIL: expected url ending with "${expectedUrlSuffix}", got "${url}"`);
      testRunner.completeTest();
      return null;
    }
    return event;
  }

  async function waitForInterceptionEventAndContinue(expectedUrlSuffix) {
    const event = await waitForInterceptionEvent(expectedUrlSuffix);
    if (!event)
      return;
    logInterceptionEvent(event);
    testRunner.log(`Continuing a request to ${event.params.request.url}`);
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
  }

  function waitForResponseReceivedEvent(fileName) {
    var promise = responseReceivedEventProimseForFile.get(fileName);
    if (!promise) {
      promise = new Promise(resolve => {
        responseReceivedResolverForFile.set(fileName, resolve);
      });
      responseReceivedEventProimseForFile.set(fileName, promise);
    }
    return promise;
  }

  async function logInterceptionEvent(event) {
    testRunner.log('Request Intercepted: ' + event.params.request.url.split('/').pop());
    testRunner.log('  responseErrorReason: ' + event.params.responseErrorReason);
    testRunner.log('  responseStatusCode: ' + event.params.responseStatusCode);
    var responseHeaders = event.params.responseHeaders;
    if (responseHeaders) {
      testRunner.log('  responseHeaders:');
      for (var headerName of Object.keys(event.params.responseHeaders).sort()) {
        var headerValue = event.params.responseHeaders[headerName].split(';')[0]; // Sometimes "; charset=UTF-8" gets in here.
        if (headersHideList.has(headerName.toLowerCase()))
          continue;
        if (headersMaskList.has(headerName.toLowerCase()))
          headerValue = '<Masked>';
        testRunner.log(`    ${headerName}: ${headerValue}`);
      }
    } else {
      testRunner.log('  responseHeaders: <None>');
    }

    if (event.params.redirectUrl)
      testRunner.log('  redirectUrl: ' + event.params.redirectUrl.split('/').pop());

    var bodyData = await session.protocol.Network.getResponseBodyForInterception({interceptionId: event.params.interceptionId});
    if (bodyData.error) {
      testRunner.log('  responseBody:');
      testRunner.log('    Error<' + bodyData.error.message + '>');
    } else {
      testRunner.log('  responseBody:');
      testRunner.log(bodyData.result.base64Encoded ? atob(bodyData.result.body) : bodyData.result.body);
    }
  }
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var headersMaskList = new Set(['date', 'server', 'last-modified', 'etag', 'keep-alive', 'x-powered-by', 'expires']);
  var headersHideList = new Set(['x-powered-by']);

  var requestInterceptedDict = {
    'ping-redirect.php': async event => {
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
        testRunner.log('    error: ' + bodyData.error.message);
      } else {
        testRunner.log('  responseBody:');
        testRunner.log(bodyData.result.base64Encoded ? atob(bodyData.result.body) : bodyData.result.body);
      }
      if (event.params.redirectUrl) {
        var body = '<html>\n<body>This content was rewritten!</body>\n</html>';
        var dummyHeaders = [
          'HTTP/1.1 200 OK',
          'Date: ' + (new Date()).toUTCString(),
          'Connection: closed',
          'Content-Length: ' + body.size,
          'Content-Type: text/html'
        ];
        testRunner.log('Modifying request with new body.');
        helper.modifyRequest(event, {rawResponse: btoa(dummyHeaders.join('\r\n') + '\r\n\r\n' + body)});
      } else {
        testRunner.log('Continue request unchanged.');
        helper.allowRequest(event);
      }
      testRunner.log('');
    }
  };

  await helper.startInterceptionTest(requestInterceptedDict, Infinity, 'Both');

  var requestId = '';
  session.protocol.Network.onRequestWillBeSent(event => requestId = event.params.requestId);
  await new Promise(resolve => {
    session.protocol.Network.onResponseReceived(resolve);
    session.evaluate(`
      fetch('${testRunner.url('../resources/ping-redirect.php')}').then(r => r.text());
    `);
  });

  var result = await session.protocol.Network.getResponseBody({requestId: requestId});
  testRunner.log('Body content received by renderer:');
  testRunner.log(result.result.base64Encoded ? atob(result.result.body) : result.result.body);

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var headersMaskList = new Set(['date', 'server', 'last-modified', 'etag', 'keep-alive']);

  var requestInterceptedDict = {
    'simple-iframe.html': async event => {
      testRunner.log('Request Intercepted: ' + event.params.request.url.split('/').pop());
      testRunner.log('  responseStatusCode: ' + event.params.responseStatusCode);
      testRunner.log('  responseHeaders:');
      for (var headerName of Object.keys(event.params.responseHeaders).sort()) {
        var headerValue = event.params.responseHeaders[headerName];
        if (headersMaskList.has(headerName.toLowerCase()))
          headerValue = '<Masked>';
        testRunner.log(`    ${headerName}: ${headerValue}`);
      }
      var bodyData = await session.protocol.Network.getResponseBodyForInterception({interceptionId: event.params.interceptionId});
      testRunner.log('  responseBody:');
      testRunner.log(bodyData.result.base64Encoded ? atob(bodyData.result.body) : bodyData.result.body);
      var body = '<html>\n<body>This content was rewritten!</body>\n</html>';
      var dummyHeaders = [
        'HTTP/1.1 200 OK',
        'Date: ' + (new Date()).toUTCString(),
        'Connection: closed',
        'Content-Length: ' + body.size,
        'Content-Type: text/html'
      ];
      helper.modifyRequest(event, {rawResponse: btoa(dummyHeaders.join('\r\n') + '\r\n\r\n' + body)});
      testRunner.log('');
    }
  };

  await helper.startInterceptionTest(requestInterceptedDict, Infinity, 'HeadersReceived');

  var requestId = '';
  session.protocol.Network.onRequestWillBeSent(event => {
    if (requestId)
      throw "requestId already set";
    requestId = event.params.requestId;
  });
  await new Promise(resolve => {
    session.protocol.Network.onLoadingFinished(resolve);
    session.evaluate(`
      var iframe = document.createElement('iframe');
      iframe.src = '${testRunner.url('./resources/simple-iframe.html')}';
      document.body.appendChild(iframe);
    `);
  });

  var result = await session.protocol.Network.getResponseBody({requestId: requestId});
  testRunner.log('Body content received by renderer:');
  testRunner.log(result.result.base64Encoded ? atob(result.result.body) : result.result.body);

  testRunner.completeTest();
})

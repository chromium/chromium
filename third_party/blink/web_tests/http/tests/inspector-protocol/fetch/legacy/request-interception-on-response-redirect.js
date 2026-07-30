(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper =
      await testRunner.loadScript('resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var headersMaskList = new Set([
    'date', 'server', 'last-modified', 'etag', 'keep-alive', 'x-powered-by',
    'expires'
  ]);
  var headersHideList = new Set(['x-powered-by']);

  var requestInterceptedDict = {
    'ping-redirect.php': async event => {
      testRunner.log('Request Intercepted: ' +
                     event.params.request.url.split('/').pop());
      testRunner.log('  responseErrorReason: ' +
                     event.params.responseErrorReason);
      testRunner.log('  responseStatusCode: ' +
                     event.params.responseStatusCode);
      var responseHeaders = event.params.responseHeaders;
      if (responseHeaders) {
        testRunner.log('  responseHeaders:');
        responseHeaders.sort((a, b) => a.name.localeCompare(b.name));
        for (const {name, value} of responseHeaders) {
          let headerValue =
              value.split(';')[0];  // Sometimes "; charset=UTF-8" gets in here.
          if (headersHideList.has(name.toLowerCase()))
            continue;
          if (headersMaskList.has(name.toLowerCase()))
            headerValue = '<Masked>';
          testRunner.log(`    ${name}: ${headerValue}`);
        }
      } else {
        testRunner.log('  responseHeaders: <None>');
      }

      if (event.params.redirectUrl)
        testRunner.log('  redirectUrl: ' +
                       event.params.redirectUrl.split('/').pop());

      var bodyData = await session.protocol.Fetch.getResponseBody(
          {requestId: event.params.requestId});
      if (bodyData.error) {
        testRunner.log('  responseBody:');
        testRunner.log('    error: ' + bodyData.error.message);
      } else {
        testRunner.log('  responseBody:');
        testRunner.log(bodyData.result.base64Encoded ?
                           atob(bodyData.result.body) :
                           bodyData.result.body);
      }
      if (event.params.redirectUrl) {
        const body =
            '<html>\n<body>This content was rewritten!</body>\n</html>';
        const dummyHeaders = [
          {name: 'Date', value: (new Date()).toUTCString()},
          {name: 'Connection', value: 'closed'},
          {name: 'Content-Length', value: String(body.length)},
          {name: 'Content-Type', value: 'text/html'}
        ];
        testRunner.log('Modifying request with new body.');
        helper.modifyRequest(event, {responseHeaders: dummyHeaders, body});
      } else {
        testRunner.log('Continue request unchanged.');
        helper.allowRequest(event);
      }
      testRunner.log('');
    }
  };

  await helper.startInterceptionTest(requestInterceptedDict, Infinity, 'Both');

  var requestId = '';
  session.protocol.Network.onRequestWillBeSent(event => requestId =
                                                   event.params.requestId);
  await new Promise(resolve => {
    session.protocol.Network.onResponseReceived(resolve);
    session.evaluate(`
      fetch('${
        testRunner.url(
            '../../resources/ping-redirect.php')}').then(r => r.text());
    `);
  });

  var result =
      await session.protocol.Network.getResponseBody({requestId: requestId});
  testRunner.log('Body content received by renderer:');
  testRunner.log(result.result.base64Encoded ? atob(result.result.body) :
                                               result.result.body);

  testRunner.completeTest();
})

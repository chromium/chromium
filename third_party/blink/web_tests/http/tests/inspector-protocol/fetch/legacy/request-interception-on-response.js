(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper =
      await testRunner.loadScript('resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var headersMaskList =
      new Set(['date', 'server', 'last-modified', 'etag', 'keep-alive']);

  var requestInterceptedDict = {
    'simple-iframe.html': async event => {
      testRunner.log('Request Intercepted: ' +
                     event.params.request.url.split('/').pop());
      testRunner.log('  responseStatusCode: ' +
                     event.params.responseStatusCode);
      testRunner.log('  responseHeaders:');
      event.params.responseHeaders.sort((a, b) => a.name.localeCompare(b.name));
      for (const {name, value} of event.params.responseHeaders) {
        let headerValue = value;
        if (headersMaskList.has(name.toLowerCase()))
          headerValue = '<Masked>';
        testRunner.log(`    ${name}: ${headerValue}`);
      }
      var bodyData = await session.protocol.Fetch.getResponseBody(
          {requestId: event.params.requestId});
      testRunner.log('  responseBody:');
      testRunner.log(bodyData.result.base64Encoded ?
                         atob(bodyData.result.body) :
                         bodyData.result.body);
      const body = '<html>\n<body>This content was rewritten!</body>\n</html>';
      const dummyHeaders = [
        {name: 'Date', value: (new Date()).toUTCString()},
        {name: 'Connection', value: 'closed'},
        {name: 'Content-Length', value: String(body.length)},
        {name: 'Content-Type', value: 'text/html'}
      ];
      helper.modifyRequest(event, {responseHeaders: dummyHeaders, body});
      testRunner.log('');
    }
  };

  await helper.startInterceptionTest(requestInterceptedDict, Infinity,
                                     'HeadersReceived');

  var requestId = '';
  session.protocol.Network.onRequestWillBeSent(event => {
    if (requestId)
      throw 'requestId already set';
    requestId = event.params.requestId;
  });
  await new Promise(resolve => {
    session.protocol.Network.onLoadingFinished(resolve);
    session.evaluate(`
      var iframe = document.createElement('iframe');
      iframe.src = '${
        testRunner.url('../../network/resources/simple-iframe.html')}';
      document.body.appendChild(iframe);
    `);
  });

  var result =
      await session.protocol.Network.getResponseBody({requestId: requestId});
  testRunner.log('Body content received by renderer:');
  testRunner.log(result.result.base64Encoded ? atob(result.result.body) :
                                               result.result.body);

  testRunner.completeTest();
})

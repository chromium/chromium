(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests a mocking of a navigation fetch.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'redirect-iframe.html': event => {
      var rawResponse =
          'HTTP/1.1 200 OK\r\n' +
          'Content-Type: text/html; charset=UTF-8\r\n\r\n' +
          '<html><head><script>' +
          'console.log("Hello from the mocked iframe.")' +
          '</' + 'script></head></html>';
      helper.mockResponse(event, rawResponse);
    },
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/redirect-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

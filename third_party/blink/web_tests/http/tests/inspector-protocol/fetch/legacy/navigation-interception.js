(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank(`Tests a mocking of a navigation fetch.`);

  var InterceptionHelper =
      await testRunner.loadScript('resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'redirect-iframe.html': event => {
      helper.mockResponse(event, {
        responseHeaders:
            [{name: 'Content-Type', value: 'text/html; charset=UTF-8'}],
        body: '<html><head><script>' +
            'console.log("Hello from the mocked iframe.")' +
            '</' +
            'script></head></html>'
      });
    },
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${
      testRunner.url('../../network/resources/redirect-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

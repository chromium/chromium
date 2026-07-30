(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank(`Tests interception of redirects.`);

  var InterceptionHelper =
      await testRunner.loadScript('resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'redirect-iframe.html': event => helper.allowRequest(event),
    'redirect1.pl': event => helper.allowRequest(event),
    'redirect2.pl': event => helper.allowRequest(event),
    'redirect3.pl': event => {
      helper.mockResponse(event, {
        responseHeaders:
            [{name: 'Content-Type', value: 'application/javascript'}],
        body: 'console.log("Hello from the mock resource");'
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

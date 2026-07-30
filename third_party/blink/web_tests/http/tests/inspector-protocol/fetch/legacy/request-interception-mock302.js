(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank(`Tests a mock 302 resource.`);

  var InterceptionHelper =
      await testRunner.loadScript('resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'redirect-iframe.html': event => helper.allowRequest(event),
    'redirect1.pl': event => helper.mockResponse(event, {
      responseCode: 302,
      responsePhrase: 'Found',
      responseHeaders: [{name: 'Location', value: 'final.js'}]
    }),
    'final.js': event => helper.allowRequest(event),
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${
      testRunner.url('../../network/resources/redirect-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper =
      await testRunner.loadScript('resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'resource-iframe.html': event => helper.allowRequest(event),
    'i-dont-exist.css': event => helper.modifyRequest(event, {url: 'test.css'}),
    'script.js': event => helper.blockRequest(event, 'ConnectionFailed'),
    'script2.js': event => {
      helper.mockResponse(event, {
        responseHeaders:
            [{name: 'Content-Type', value: 'application/javascript'}],
        body: 'console.log("Hello from the mock resource");'
      });
    },
    'post-echo.pl': event => helper.allowRequest(event),
  };

  await helper.startInterceptionTest(requestInterceptedDict, 2);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${
      testRunner.url('../../network/resources/resource-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

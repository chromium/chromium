(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests canceling an HTTP auth challenge via DevTools protocol.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
      'iframe-auth-js.html': event => helper.allowRequest(event),
      'unauthorised.pl': event => helper.allowRequest(event),
      'unauthorised.pl+Auth': event => helper.cancelAuth(event),
  };

  await helper.startInterceptionTest(requestInterceptedDict, 0);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/iframe-auth-js.html')}';
    document.body.appendChild(iframe);
  `);
})

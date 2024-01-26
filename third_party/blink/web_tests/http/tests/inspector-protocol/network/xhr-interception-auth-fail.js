(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception of an XHR request that fails due to lack of credentials.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'xhr-iframe-auth-fail.html': event => helper.allowRequest(event),
    'unauthorised.pl': event => helper.allowRequest(event),
    'unauthorised.pl+Auth': event => helper.defaultAuth(event),
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/xhr-iframe-auth-fail.html')}';
    document.body.appendChild(iframe);
  `);
})

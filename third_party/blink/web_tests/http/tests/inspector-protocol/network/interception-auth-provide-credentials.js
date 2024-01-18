(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests providing HTTP auth credentials over DevTools protocol.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'iframe-auth-js.html': event => helper.allowRequest(event),
    'unauthorised.pl': event => helper.allowRequest(event),
    'unauthorised.pl+Auth': event => helper.provideAuthCredentials(event, 'TestUser', 'TestPassword'),
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/iframe-auth-js.html')}';
    document.body.appendChild(iframe);
  `);
})

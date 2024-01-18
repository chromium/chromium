(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception of redirects.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'redirect-iframe.html': event => helper.allowRequest(event),
    'redirect1.pl': event => helper.allowRequest(event),
    'redirect2.pl': event => helper.allowRequest(event),
    'redirect3.pl': event => helper.modifyRequest(event, {url: 'alternative.js'}),
    'alternative.js': event => helper.allowRequest(event)
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/redirect-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
      'disable-iframe.html': event => helper.allowRequest(event),
      'i-dont-exist.css': event => helper.disableRequestInterception(event),
      'post-echo.pl': event => helper.allowRequest(event),
  };

  // The XHR triggered in the onload handler of the iframe races with the frameStoppedLoading event
  // for the frame, so don't record it in the trace.
  helper.setSilentFrameStoppedLoading(true);
  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/disable-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

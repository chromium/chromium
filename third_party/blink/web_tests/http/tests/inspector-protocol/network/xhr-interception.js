(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests interception of an XHR request.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var requestInterceptedDict = {
    'xhr-iframe.html': event => helper.allowRequest(event),
    'example.txt': event => helper.mockResponse(event, 'HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nPayload for the Mock XHR response;'),
  };

  // The XHR triggered in the onload handler of the iframe races with the frameStoppedLoading event
  // for the frame, so don't record it in the trace.
  helper.setSilentFrameStoppedLoading(true);
  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/xhr-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})

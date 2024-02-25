(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the browser does not crash while intercepting a request where blob is posted (crbug.com/782545)`);

  session.protocol.Network.enable();
  session.protocol.Page.enable();

  session.protocol.Network.onRequestIntercepted(event => {
    session.protocol.Network.continueInterceptedRequest({interceptionId: event.params.interceptionId});
  });
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*"}]});

  await session.evaluateAsync(`(function() {
    var blob = new Blob(['data']);
    var url = '${testRunner.url('./resources/post-echo.pl')}';
    return fetch(new Request(url, {method: 'POST', body: blob}));
  })()`);
  testRunner.completeTest();
})

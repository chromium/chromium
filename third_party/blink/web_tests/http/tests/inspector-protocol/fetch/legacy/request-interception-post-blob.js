(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the browser does not crash while intercepting a request where blob is posted (crbug.com/782545)`);

  session.protocol.Network.enable();
  session.protocol.Page.enable();

  session.protocol.Fetch.onRequestPaused(event => {
    session.protocol.Fetch.continueRequest({requestId: event.params.requestId});
  });
  await session.protocol.Fetch.enable({patterns: [{urlPattern: '*'}]});

  await session.evaluateAsync(`(function() {
    var blob = new Blob(['data']);
    var url = '${testRunner.url('../../network/resources/post-echo.pl')}';
    return fetch(new Request(url, {method: 'POST', body: blob}));
  })()`);
  testRunner.completeTest();
})

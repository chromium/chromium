(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that intercepted resonses can set cookies.`);

  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Fetch.enable({patterns: [{}]});
  dp.Fetch.onRequestPaused(e => {
    dp.Fetch.fulfillRequest({
      requestId: e.params.requestId,
      responseCode: 200,
      responseHeaders: [
        {name: 'Set-Cookie', value: 'my_special_cookie=no_domain'},
        {name: 'Content-Type', value: 'text/html'}
      ],
      body: btoa('<html>Hello world</html>')
    });
  });

  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/inspector-protocol/network/resources/simple.html'
  });
  await dp.Network.onceLoadingFinished();

  testRunner.log(`cookie: ${(await session.evaluate('document.cookie'))}`);
  testRunner.completeTest();
})

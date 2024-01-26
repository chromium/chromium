(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that intercepted resonses can set cookies.`);

  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Network.setRequestInterception({patterns: [{}]});
  dp.Network.onRequestIntercepted(e => {
    const response = [
        'HTTP/1.1 200 OK',
        'Set-Cookie: my_special_cookie=no_domain',
        'Content-Type: text/html',
        '',
        '<html>Hello world</html>'];
    const rawResponse = btoa(response.join('\r\n'));
    dp.Network.continueInterceptedRequest({interceptionId: e.params.interceptionId, rawResponse});
  });

  dp.Page.navigate({url: 'http://127.0.0.1:8000/inspector-protocol/network/resources/simple.html'});
  await dp.Network.onceLoadingFinished();

  testRunner.log(`cookie: ${(await session.evaluate("document.cookie"))}`);
  testRunner.completeTest();
})

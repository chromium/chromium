(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that request timing is that of latest request when response is overriden.`);

  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Network.setRequestInterception({patterns: [{}]});
  dp.Network.onRequestIntercepted(e => {
    const params = e.params;
    let response;
    testRunner.log(`intercepted ${params.request.url}`);
    if (/before-redirect$/.test(params.request.url)) {
      response = [
        'HTTP/1.1 303 See Other',
        'Location: http://example.com/after-redirect',
        '',''];
    } else {
      response = [
        'HTTP/1.1 200 OK',
        'Content-Type: text/html',
        '',
        '<html>Hello world</html>'];
    }
    const rawResponse = btoa(response.join('\r\n'));
    dp.Network.continueInterceptedRequest({interceptionId: e.params.interceptionId, rawResponse});
  });

  const requestTimes = [];
  dp.Network.onRequestWillBeSent(e => {
    requestTimes.push(e.params.timestamp);
  });

  dp.Page.navigate({url: 'http://example.com/before-redirect'});
  const responseReceived = (await dp.Network.onceResponseReceived()).params;
  const timing = responseReceived.response.timing;
  testRunner.log(`Total requests sent: ${requestTimes.length}`);
  if (requestTimes[1] < timing.requestTime)
    testRunner.log('PASS');
  else
    testRunner.log(`FAIL: request sent in the past, ${requestTimes[1]} vs. ${timing.requestTime}`);

  testRunner.completeTest();
})

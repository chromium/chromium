(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that request timing is that of latest request when response is overriden.`);

  await session.protocol.Network.enable();
  await session.protocol.Runtime.enable();

  await dp.Fetch.enable({patterns: [{}]});
  dp.Fetch.onRequestPaused(e => {
    const params = e.params;
    testRunner.log(`intercepted ${params.request.url}`);
    if (/before-redirect$/.test(params.request.url)) {
      dp.Fetch.fulfillRequest({
        requestId: params.requestId,
        responseCode: 303,
        responsePhrase: 'See Other',
        responseHeaders:
            [{name: 'Location', value: 'http://example.com/after-redirect'}]
      });
    } else {
      dp.Fetch.fulfillRequest({
        requestId: params.requestId,
        responseCode: 200,
        responsePhrase: 'OK',
        responseHeaders: [{name: 'Content-Type', value: 'text/html'}],
        body: btoa('<html>Hello world</html>')
      });
    }
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
    testRunner.log(`FAIL: request sent in the past, ${requestTimes[1]} vs. ${
        timing.requestTime}`);

  testRunner.completeTest();
})

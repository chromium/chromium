(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the overriden referer headers are properly reported and applied with interception`);

  session.protocol.Network.enable();
  session.protocol.Page.enable();
  await dp.Network.setExtraHTTPHeaders(
      {headers: {'ReFeReR': 'https://127.0.0.1:8000/'}});
  await session.protocol.Fetch.enable({patterns: [{urlPattern: '*'}]});

  testRunner.log('*Not* overriding referer in interception handler:');
  var {requestId, bodyPromise} = await sendRequestAndIntercept();
  session.protocol.Fetch.continueRequest({requestId: requestId});
  testRunner.log(`response: ${await bodyPromise}`);

  testRunner.log('Overriding referer in interception handler:');
  var {requestId, bodyPromise} = await sendRequestAndIntercept();
  session.protocol.Fetch.continueRequest({
    requestId: requestId,
    headers: [{name: 'ReFeReR', value: 'http://localhost:8000/'}]
  });
  testRunner.log(`response: ${await bodyPromise}`);

  testRunner.completeTest();

  async function sendRequestAndIntercept() {
    const requestPromise = session.protocol.Network.onceRequestWillBeSent();
    const evalPromise = session.evaluateAsync(`(async function() {
      var url = '${
        testRunner.url(
            '../../network/resources/echo-headers.php?headers=HTTP_REFERER')}';
      var response = await fetch(new Request(url));
      return response.text();
    })()`);
    const interceptedRequest =
        (await session.protocol.Fetch.onceRequestPaused()).params;
    const request = (await requestPromise).params;
    testRunner.log(
        `referer in requestWillBeSent: ${request.request.headers['Referer']}`);
    testRunner.log(`referer in requestIntercepted: ${
        interceptedRequest.request.headers['Referer']}`);
    return {requestId: interceptedRequest.requestId, bodyPromise: evalPromise};
  }
})

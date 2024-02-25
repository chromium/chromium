(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the overriden referer headers are properly reported and applied with interception`);

  session.protocol.Network.enable();
  session.protocol.Page.enable();
  await dp.Network.setExtraHTTPHeaders({headers: {'ReFeReR': 'https://127.0.0.1:8000/'}});
  await session.protocol.Network.setRequestInterception({patterns: [{urlPattern: "*"}]});

  testRunner.log('*Not* overriding referer in interception handler:');
  var {interceptionId, bodyPromise} = await sendRequestAndIntercept();
  session.protocol.Network.continueInterceptedRequest({interceptionId: interceptionId});
  testRunner.log(`response: ${await bodyPromise}`);

  testRunner.log('Overriding referer in interception handler:');
  var {interceptionId, bodyPromise} = await sendRequestAndIntercept();
  session.protocol.Network.continueInterceptedRequest({interceptionId: interceptionId, headers: {'ReFeReR': 'http://localhost:8000/'}});
  testRunner.log(`response: ${await bodyPromise}`);

  testRunner.completeTest();

  async function sendRequestAndIntercept() {
    const requestPromise = session.protocol.Network.onceRequestWillBeSent();
    const evalPromise = session.evaluateAsync(`(async function() {
      var url = '${testRunner.url('./resources/echo-headers.php?headers=HTTP_REFERER')}';
      var response = await fetch(new Request(url));
      return response.text();
    })()`);
    const interceptedRequest = (await session.protocol.Network.onceRequestIntercepted()).params;
    const request = (await requestPromise).params;
    testRunner.log(`referer in requestWillBeSent: ${request.request.headers['Referer']}`);
    testRunner.log(`referer in requestIntercepted: ${interceptedRequest.request.headers['Referer']}`);
    return {interceptionId: interceptedRequest.interceptionId, bodyPromise: evalPromise};
  }
})

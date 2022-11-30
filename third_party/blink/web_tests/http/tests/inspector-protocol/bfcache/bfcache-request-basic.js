(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
    'http://localhost:8000/inspector-protocol/resources/test-page.html',
    'Tests that no network requests are logged for a BFCache navigation');
  await dp.Network.enable();
  await dp.Page.enable();

  // Regular navigation.
  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  await session.navigate('https://devtools.oopif.test:8443/inspector-protocol/resources/iframe.html');
  const requestParams = (await requestWillBeSentPromise).params;
  const responseParams = (await responseReceivedPromise).params;
  testRunner.log(`request for ${requestParams.documentURL} will be sent`);
  testRunner.log(`response code: ${responseParams.response.status}`);

  // No network requests should be reported for back-forward cache navigations.
  dp.Network.onceRequestWillBeSent().then(request => {
    testRunner.fail(`request for ${request.params.documentURL} will be sent`);
  });
  dp.Network.onceResponseReceived().then(response => {
    testRunner.fail(`response for ${request.params.documentURL} was received`);
  });

  // Navigate back - should use back-forward cache.
  session.evaluate('window.history.back()');
  const frameNavigated = await dp.Page.onceFrameNavigated();
  testRunner.log(frameNavigated.params.type);
  testRunner.completeTest();
});

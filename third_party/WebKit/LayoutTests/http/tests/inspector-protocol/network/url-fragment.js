(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the we properly report url fragment in network requests`);

  dp.Network.enable();
  dp.Page.enable();

  async function dumpEventsAndContinue() {
    const requestInterceptedPromise = dp.Network.onceRequestIntercepted();
    const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
    const interceptedRequestParams =  (await requestInterceptedPromise).params;
    const interceptedRequest = interceptedRequestParams.request;
    const sentRequest = (await requestWillBeSentPromise).params.request;

    testRunner.log(`Request will be sent: ${sentRequest.url} fragment: ${sentRequest.urlFragment}`);
    testRunner.log(`Intercepted URL: ${interceptedRequest.url} fragment: ${interceptedRequest.urlFragment}`);
    dp.Network.continueInterceptedRequest({interceptionId: interceptedRequestParams.interceptionId});
  }

  await dp.Network.setRequestInterception({patterns: [{}]});
  const navigatePromise = dp.Page.navigate({url: 'http://127.0.0.1:8000/devtools/network/resources/empty.html#ref'});
  await dumpEventsAndContinue();
  await navigatePromise;
  const resourceURL = '/devtools/network/resources/resource.php';
  session.evaluateAsync(`fetch("${resourceURL}")`);
  await dumpEventsAndContinue();
  session.evaluateAsync(`fetch("${resourceURL}#ref")`);
  dumpEventsAndContinue();
  session.evaluateAsync(`fetch("${resourceURL}#")`);
  await dumpEventsAndContinue();
  testRunner.completeTest();
})

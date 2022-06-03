(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.navigate() returns the results that match referrer-policy configs`);

  await dp.Page.enable();
  await dp.Network.enable();
  var navigatePromise = dp.Page.navigate({
    url: 'http://example.com',
    referrer: 'https://www.google.com/search?q=bing',
    referrerPolicy: 'origin',
  });
  var requestWillBeSent = dp.Network.onRequestWillBeSent(event => {
    const request = event.params.request;
    testRunner.log('Request referrer: ' + request.headers.Referer);
    testRunner.log('Request referrer-policy: ' + request.referrerPolicy);
  });
  const params = (await dp.Network.onceRequestWillBeSent()).params;
  testRunner.completeTest();
})

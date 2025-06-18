(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startBlank(
      `Tests that Page.navigate() returns the results that match referrer-policy configs`);

  await dp.Page.enable();
  await dp.Network.enable();

  testRunner.log(await dp.Page.navigate({
    url: 'http://example.com',
    referrer: 'https://www.google.com/search?q=bing',
    referrerPolicy: 'no-referrer',
  }), 'Invalid policy: ');

  dp.Page.navigate({
    url: 'http://example.com',
    referrer: 'https://www.google.com/search?q=bing',
    referrerPolicy: 'origin',
  });
  dp.Network.onRequestWillBeSent(event => {
    const request = event.params.request;
    testRunner.log('Request referrer: ' + request.headers.Referer);
    testRunner.log('Request referrer-policy: ' + request.referrerPolicy);
  });
  await dp.Network.onceRequestWillBeSent();
  testRunner.completeTest();
})

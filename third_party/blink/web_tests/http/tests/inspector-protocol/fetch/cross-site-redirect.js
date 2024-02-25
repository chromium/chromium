(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that fetch correctly follows cross-site redirects.`);

  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, testRunner.browserP());
  await helper.enable();

  // Redirect chain: a.test -> b.test -> c.test
  helper.onceRequest(/a.test/).fulfill({
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: 'https://b.test'},
    ]
  });
  helper.onceRequest(/b.test/).fulfill({
    responseCode: 302,
    responseHeaders: [
      {name: 'Location', value: 'https://c.test'},
    ]
  });

  // Navigate from start.test to a.test to kick off the redirect chain.
  helper.onceRequest(/start.test/).fulfill({
    responseCode: 200,
    body: btoa("<html></html>")
  });

  await dp.Page.enable();
  await dp.Page.navigate({url: 'https://start.test'});
  session.evaluate(`location.href = 'https://a.test'`);
  const request = await helper.onceRequest(/c.test/).matched();
  dp.Fetch.fulfillRequest({requestId: request.requestId, responseCode: 200});

  testRunner.completeTest();
})


(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests fetching font response body.`);

  await dp.Network.enable();

  async function run(pageUrl, fontUrl) {
    session.navigate(pageUrl);

    testRunner.log('Waiting for request...');
    const event = await dp.Network.onceRequestWillBeSent(e => {
      return e.params.request.url.includes(fontUrl);
    });
    testRunner.log('Waiting for loading finished...');
    await dp.Network.onceLoadingFinished(e => {
      return e.params.requestId === event.params.requestId;
    });

    const data = await dp.Network.getResponseBody({requestId: event.params.requestId});
    if (data.result)
      data.result.body = '<body>';
    testRunner.log(data, 'Response body: ');
  }

  await run('./resources/get-response-body-font.html', 'Ahem.ttf');
  await run('./resources/get-response-body-font-utf8.html', 'get-response-body-font-utf8.php');

  testRunner.completeTest();
})

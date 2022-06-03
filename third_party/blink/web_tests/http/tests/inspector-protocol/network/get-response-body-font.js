(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests fetching font response body.`);

  await dp.Network.enable();

  session.navigate('./resources/get-response-body-font.html');

  testRunner.log('Waiting for request...');
  const event = await dp.Network.onceRequestWillBeSent(e => {
    return e.params.request.url.includes('Ahem.ttf');
  });
  testRunner.log('Waiting for loading finished...');
  await dp.Network.onceLoadingFinished(e => {
    return e.params.requestId === event.params.requestId;
  });

  const data = await dp.Network.getResponseBody({requestId: event.params.requestId});
  if (data.result)
    data.result.body = '<body>';
  testRunner.log(data, 'Response body: ');

  testRunner.completeTest();
})

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests fetching of a response body.`);

  await dp.Network.enable();

  async function logResponseBody(url) {
    session.evaluate(`fetch(${JSON.stringify(url)}).then(r => r.text());`);

    var requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
    testRunner.log(`Request for ${requestWillBeSent.request.url}`);

    await dp.Network.onceResponseReceived();
    var data = await dp.Network.getResponseBody({requestId: requestWillBeSent.requestId});
    testRunner.log(data.result.body);
  }

  await logResponseBody(testRunner.url('./resources/final.js'));
  await logResponseBody(testRunner.url('./resources/test.css'));

  testRunner.completeTest();
})

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests searcing in a response body.`);
  await dp.Network.enable();

  var url = testRunner.url('./resources/final.js');
  session.evaluate(`fetch("${url}").then(r => r.text());`);

  var requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
  testRunner.log(`Request for ${requestWillBeSent.request.url}`);
  await dp.Network.onceResponseReceived();
  var data = await dp.Network.searchInResponseBody({
    requestId: requestWillBeSent.requestId,
    query: 'hello'
  });
  testRunner.log(data.result);
  testRunner.completeTest();
})

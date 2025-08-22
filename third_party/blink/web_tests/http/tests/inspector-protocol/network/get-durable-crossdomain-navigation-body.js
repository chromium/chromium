(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that DurableMessages live after a cross-origin navigation`);

  await dp.Network.enable({maxTotalBufferSize: 115025, enableDurableMessages: true});
  await dp.Page.enable();

  const resourceUrl = testRunner.url('./resources/final.js');
  session.evaluate(`fetch("${resourceUrl}").then(r => r.text());`);
  const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
  testRunner.log(`Request for ${requestWillBeSent.request.url}`);
  await dp.Network.onceResponseReceived();
  const resourceRequestId = requestWillBeSent.requestId;

  testRunner.log('-- Test Page.navigate() to a cross origin URL --');
  await session.navigate('https://second.test/resources/hello-world.html');

  const data = await dp.Network.getResponseBody({requestId: resourceRequestId});
  testRunner.log(btoa(encodeURIComponent(data.result.body)));

  testRunner.completeTest();
})



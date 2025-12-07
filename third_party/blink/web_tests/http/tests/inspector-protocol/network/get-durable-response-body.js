(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests fetching of a response body via DurableMessages mechanism.`);

  await dp.Network.enable({maxTotalBufferSize: 115025, enableDurableMessages: true});

  async function logResponseBody(url) {
    session.evaluate(`fetch("${url}").then(r => r.text());`);

    const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
    testRunner.log(`Request for ${requestWillBeSent.request.url}`);

    await dp.Network.onceResponseReceived();
    const data = await dp.Network.getResponseBody({requestId: requestWillBeSent.requestId});
    testRunner.log(btoa(encodeURIComponent(data.result.body)));
  }

  await logResponseBody(testRunner.url('./resources/final.js'));
  await logResponseBody(testRunner.url('./resources/test.css'));
  await logResponseBody(testRunner.url('./resources/abe.png'));

  testRunner.completeTest();
})

(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Ensures that DurableMessages can be reconfigured with a second call`);

  await dp.Network.enable({maxTotalBufferSize: 10, enableDurableMessages: true});

  async function logResponseBody(url) {
    session.evaluate(`fetch("${url}").then(r => r.text());`);

    const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
    testRunner.log(`Request for ${requestWillBeSent.request.url}`);

    await dp.Network.onceResponseReceived();
    const data = await dp.Network.getResponseBody({requestId: requestWillBeSent.requestId});
    testRunner.log(btoa(encodeURIComponent(data.result.body)));
  }

  await logResponseBody(testRunner.url('./resources/final.js'));

  await dp.Network.enable({maxTotalBufferSize: 100, enableDurableMessages: true});

  await logResponseBody(testRunner.url('./resources/final.js'));

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that requestIntercepted has a requestId corresponding to requestWillBeSent's requestId`);

  await dp.Network.enable();
  await dp.Fetch.enable({patterns: [{urlPattern: '*'}]});

  const [requestWillBeSent, requestIntercepted, evaluate] = await Promise.all([
    dp.Network.onceRequestWillBeSent(), dp.Fetch.onceRequestPaused(),
    session.evaluate(
        `fetch('${testRunner.url('../../network/resources/test.css')}')`)
  ]);

  const idsAreEqual = requestWillBeSent.params.requestId ===
      requestIntercepted.params.networkId;
  testRunner.log(
      `requestIntercepted.requestId === requestWillBeSent.requestId: ${
          idsAreEqual}`);
  testRunner.completeTest();
})

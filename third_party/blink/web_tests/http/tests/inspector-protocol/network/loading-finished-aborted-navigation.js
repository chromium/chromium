(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
    "Tests that loadingFinished is correctly reported once when navigation " +
    "is aborted."
  );
  dp.Network.enable();
  await dp.Fetch.enable();

  const navigationPromise = dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/basic.html'
  });
  let count = 0;
  dp.Network.onLoadingFailed(event => {
    testRunner.log(event, count++ ? "FAILED: event not expected " : "");
  });
  const request = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`intercepted: ${request.request.url}, aborting`);

  dp.Fetch.failRequest({
    requestId: request.requestId,
    errorReason: 'Aborted'
  });

  await navigationPromise;
  // Do a round-trip to ensure second failure has a chance to arrive.
  await session.evaluate("");
  testRunner.completeTest();
})

(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
    `Tests navigation request can be fulfilled without a body.`,
  );

  const url = 'http://127.0.0.1:8000/protocol/inspector-protocol-page.html';

  await dp.Network.enable();
  await dp.Fetch.enable();

  const navigatePromise = dp.Page.navigate({ url });
  const request = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log('Network request paused.');

  dp.Fetch.fulfillRequest({
    requestId: request.requestId,
    responseCode: 200,
  });

  await Promise.all([dp.Network.onceResponseReceived(), navigatePromise]);
  testRunner.log('Network response and navigation received.');

  testRunner.completeTest();
});

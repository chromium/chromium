(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    `Test that Fetch.fulfillRequest with invalid status code does not crash`);

  await dp.Runtime.enable();
  await dp.Network.enable();
  await dp.Fetch.enable({});

  session.navigate(testRunner.url('./resources/hello-world.html'))
  const requestPaused = (await dp.Fetch.onceRequestPaused()).params;
  testRunner.log(`Intercepted request ${requestPaused.responseStatusCode}`);
  const result = await dp.Fetch.fulfillRequest({
    requestId: requestPaused.requestId,
    responseCode: 777,
  });
  testRunner.log(result?.error);
  testRunner.completeTest();
})

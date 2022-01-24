(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the IP address space is reported on responseReceivedExtraInfo.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  dp.Network.onResponseReceivedExtraInfo(event => {
    testRunner.log(event.params.statusCode);
  });

  await session.evaluateAsync(`fetch('index.html');`);
  await session.evaluateAsync(
      `fetch('/inspector-protocol/network/resources/hello-world.html');`);
  testRunner.completeTest();
})

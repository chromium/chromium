(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the IP address space is reported on responseReceivedExtraInfo.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  dp.Network.onResponseReceivedExtraInfo(event => {
    testRunner.log(event.params.resourceIPAddressSpace);
    testRunner.completeTest();
  });

  await session.evaluate(`fetch('index.html');`);
})

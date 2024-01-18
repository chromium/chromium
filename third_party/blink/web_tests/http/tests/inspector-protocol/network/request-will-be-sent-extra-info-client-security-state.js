(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that client security state is reported on requestWillBeSentExtraInfo.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  dp.Network.onRequestWillBeSentExtraInfo(event => {
    testRunner.log(event.params.clientSecurityState);
    testRunner.completeTest();
  });

  await session.evaluate(`fetch('index.html');`);
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/',
      `Verifies that Network.emulateNetworkConditions stops requests when offline is enabled.`);

  await dp.Network.enable();

  await dp.Network.emulateNetworkConditions({
    downloadThroughput: -1,
    latency: 0,
    offline: true,
    uploadThroughput: -1
  });

  session.evaluate(`fetch('/')`);
  const loadingFailed = await dp.Network.onceLoadingFailed();
  testRunner.log('loadingFailed.params.errorText: ' + loadingFailed.params.errorText);

  testRunner.log('navigator.onLine: ' + await session.evaluate('navigator.onLine'));

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Verify that overrideNetworkState sets navigator.onLine and navigator.connection');

  function networkState() {
    const {onLine, connection} = navigator;
    const {downlinkMax, effectiveType, rtt, saveData, type} =
        connection;
    return {onLine, downlinkMax, effectiveType, rtt, saveData, type};
  }

  await dp.Network.enable();
  testRunner.log(await session.evaluate(networkState));

  await dp.Network.overrideNetworkState({
    offline: false,
    latency: 50,
    downloadThroughput: 100,
    uploadThroughput: 30,
    connectionType: 'bluetooth',
  });
  testRunner.log(await session.evaluate(networkState));

  await dp.Network.overrideNetworkState({
    offline: true,
    latency: 50,
    downloadThroughput: 100,
    uploadThroughput: 30,
    connectionType: 'bluetooth',
  });
  testRunner.log(await session.evaluate(networkState));

  testRunner.completeTest();
})

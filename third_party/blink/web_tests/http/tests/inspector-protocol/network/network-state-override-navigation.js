(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'http://127.0.0.1:8000/inspector-protocol/resources/empty.html',
      `Verifies that the navigator.onLine override survives cross-document navigations.`);

  await dp.Network.enable();
  await dp.Network.overrideNetworkState({
    offline: true,
    latency: 0,
    downloadThroughput: -1,
    uploadThroughput: -1,
  });
  testRunner.log('navigator.onLine: ' +
                 await session.evaluate('navigator.onLine'));

  await session.navigate(
      'http://127.0.0.1:8000/inspector-protocol/resources/test-page.html');
  testRunner.log('navigator.onLine after same-origin navigation: ' +
                 await session.evaluate('navigator.onLine'));

  await session.navigate(
      'http://devtools.oopif.test:8000/inspector-protocol/resources/test-page.html');
  testRunner.log('navigator.onLine after cross-origin navigation: ' +
                 await session.evaluate('navigator.onLine'));

  await dp.Network.overrideNetworkState({
    offline: false,
    latency: 0,
    downloadThroughput: -1,
    uploadThroughput: -1,
  });
  testRunner.log('navigator.onLine after clearing the override: ' +
                 await session.evaluate('navigator.onLine'));

  await session.navigate(
      'http://127.0.0.1:8000/inspector-protocol/resources/test-page.html');
  testRunner.log('navigator.onLine after navigating without the override: ' +
                 await session.evaluate('navigator.onLine'));

  testRunner.completeTest();
})

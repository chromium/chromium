(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/resource.php?type=html',
      'Verifies offline emulation with socket-layer network throttling.');
  const helper = (await testRunner.loadScript('resources/helper.js'))(
      testRunner, session, dp);
  await dp.Network.enable();

  testRunner.log('Offline:');
  await helper.setConditions({offline: true});
  const loadingFailed = dp.Network.onceLoadingFailed();
  testRunner.log('fetch(): ' +
                 await helper.fetchResult(helper.resourcePath('text')));
  testRunner.log('Error: ' + (await loadingFailed).params.errorText);

  // Going offline keeps the conditions' isolated session, and so its HTTP
  // cache, as clients toggling offline keep the other conditions.
  testRunner.log('Offline while throttled:');
  await helper.setConditions({latency: helper.rttMs});
  const cachedPath = helper.resourcePath('cacheable');
  helper.logThrottling('fetch() of a cacheable resource',
                       await helper.measureFetchOf(cachedPath), true);
  await helper.setConditions({latency: helper.rttMs, offline: true});
  testRunner.log('fetch() of the cached resource: ' +
                 await helper.fetchResult(cachedPath));
  testRunner.log('fetch() of another resource: ' +
                 await helper.fetchResult(helper.resourcePath('text')));

  testRunner.log('Back online:');
  await helper.setConditions({latency: helper.rttMs});
  helper.logThrottling('fetch() of the cached resource',
                       await helper.measureFetchOf(cachedPath), false);
  helper.logThrottling('fetch() of another resource',
                       await helper.measureFetch(), true);

  testRunner.log('An offline rule with a URL pattern:');
  const offlinePath = new URL(testRunner.url('resources/offline.txt')).pathname;
  await dp.Network.emulateNetworkConditionsByRule({
    matchedNetworkConditions: [
      {
        urlPattern: '*://*:*/*/offline.txt',
        latency: 0,
        downloadThroughput: -1,
        uploadThroughput: -1,
        offline: true,
      },
      {
        urlPattern: '',
        latency: helper.rttMs,
        downloadThroughput: -1,
        uploadThroughput: -1,
      },
    ],
  });
  testRunner.log('fetch() of a matching resource: ' +
                 await helper.fetchResult(offlinePath));
  helper.logThrottling('fetch() of another resource',
                       await helper.measureFetch(), true);

  testRunner.completeTest();
})

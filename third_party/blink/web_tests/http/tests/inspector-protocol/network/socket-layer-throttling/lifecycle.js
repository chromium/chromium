(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/resource.php?type=html',
      'Verifies when socket-layer network throttling applies, and when it ' +
          'stops.');
  const helper = (await testRunner.loadScript('resources/helper.js'))(
      testRunner, session, dp);
  await dp.Network.enable();

  testRunner.log('A rejected command leaves the conditions unchanged:');
  await helper.setConditions({latency: helper.rttMs});
  const {error} = await dp.Network.emulateNetworkConditionsByRule({
    matchedNetworkConditions: [{
      urlPattern: 'http://127.0.0.1:*/*',
      latency: 0,
      downloadThroughput: 1024,
      uploadThroughput: -1,
    }],
  });
  testRunner.log(`Error: ${error.message}`);
  helper.logThrottling('fetch()', await helper.measureFetch(), true);

  testRunner.log('Detaching a session that set no conditions keeps them:');
  const idleSession = await page.createSession();
  await idleSession.disconnect();
  helper.logThrottling('fetch()', await helper.measureFetch(), true);

  // Requests are only throttled while the Network domain is enabled in a
  // session attached to the page, so another session keeps it enabled.
  testRunner.log('Network.disable clears the conditions:');
  const otherSession = await page.createSession();
  await otherSession.protocol.Network.enable();
  await dp.Network.disable();
  helper.logThrottling('fetch()', await helper.measureFetch(), false);

  testRunner.log('Detaching clears the conditions:');
  await dp.Network.enable();
  await helper.setConditions({latency: helper.rttMs}, otherSession.protocol);
  helper.logThrottling('fetch()', await helper.measureFetch(), true);
  await otherSession.disconnect();
  helper.logThrottling('fetch() after detaching', await helper.measureFetch(),
                       false);

  testRunner.log('Conditions without throttling don\'t throttle:');
  await helper.setConditions({latency: helper.rttMs});
  helper.logThrottling('fetch()', await helper.measureFetch(), true);
  await helper.setConditions({});
  helper.logThrottling('fetch()', await helper.measureFetch(), false);

  testRunner.log('The deprecated emulateNetworkConditions throttles too:');
  const deprecatedConditions = {
    offline: false,
    latency: 0,
    downloadThroughput: -1,
    uploadThroughput: -1
  };
  await dp.Network.emulateNetworkConditions(
      {...deprecatedConditions, latency: helper.rttMs});
  helper.logThrottling('fetch()', await helper.measureFetch(), true);
  await dp.Network.emulateNetworkConditions(deprecatedConditions);
  helper.logThrottling('fetch()', await helper.measureFetch(), false);

  testRunner.completeTest();
})

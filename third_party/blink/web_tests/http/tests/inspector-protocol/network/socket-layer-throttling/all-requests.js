(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/resource.php?type=html',
      'Verifies that socket-layer network throttling applies to all requests.');
  const helper = (await testRunner.loadScript('resources/helper.js'))(
      testRunner, session, dp);
  await dp.Network.enable();

  const {result: {ruleIds}} =
      await helper.setConditions({latency: helper.rttMs});
  const extraInfo = dp.Network.onceRequestWillBeSentExtraInfo();
  helper.logThrottling('fetch()', await helper.measureFetch(), true);
  testRunner.log(
      'fetch() reports the rule: ' +
      ((await extraInfo).params.appliedNetworkConditionsId === ruleIds[0]));
  helper.logThrottling('<script>', await helper.measureScript(), true);
  helper.logThrottling('Navigation', await helper.measureNavigation(), true);
  // A cross-site navigation commits in a new renderer process, and the
  // conditions follow.
  helper.logThrottling('Cross-site navigation',
                       await helper.measureNavigation('http://localhost:8000'),
                       true);
  helper.logThrottling('fetch() after it', await helper.measureFetch(), true);

  await helper.clearConditions();
  helper.logThrottling('fetch() after clearing', await helper.measureFetch(),
                       false);

  testRunner.completeTest();
})

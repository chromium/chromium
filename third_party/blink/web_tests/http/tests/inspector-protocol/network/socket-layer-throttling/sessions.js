(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/resource.php?type=html',
      'Verifies that sessions don\'t replace or clear each other\'s ' +
          'socket-layer network conditions.');
  const helper = (await testRunner.loadScript('resources/helper.js'))(
      testRunner, session, dp);
  await dp.Network.enable();
  const otherSession = await page.createSession();

  testRunner.log('Another session\'s conditions don\'t replace these:');
  await helper.setConditions({latency: helper.rttMs});
  // These only limit uploads, which small requests don't notice.
  await helper.setConditions({uploadThroughput: 100 * 1024},
                             otherSession.protocol);
  helper.logThrottling('fetch()', await helper.measureFetch(), true);

  testRunner.log('Another session clearing its conditions keeps these:');
  await helper.clearConditions(otherSession.protocol);
  helper.logThrottling('fetch()', await helper.measureFetch(), true);

  testRunner.log(
      'Once these are cleared, the next session\'s conditions apply:');
  await helper.setConditions({latency: helper.rttMs}, otherSession.protocol);
  await helper.clearConditions();
  helper.logThrottling('fetch()', await helper.measureFetch(), true);

  // These conditions come first once the other session sets its conditions
  // again, so they keep applying.
  testRunner.log('Conditions without throttling keep their place:');
  await helper.setConditions({});
  await helper.clearConditions(otherSession.protocol);
  await helper.setConditions({latency: helper.rttMs}, otherSession.protocol);
  helper.logThrottling('fetch()', await helper.measureFetch(), false);
  await helper.clearConditions();
  helper.logThrottling('fetch() once they\'re cleared',
                       await helper.measureFetch(), true);
  await otherSession.disconnect();
  helper.logThrottling('fetch() after that session detaches',
                       await helper.measureFetch(), false);

  // This session attached first, but a session attaching later sets its
  // conditions first, so they keep applying, including across a cross-site
  // navigation, which commits in a new renderer process.
  testRunner.log('Conditions keep their order across a cross-site navigation:');
  const laterSession = await page.createSession();
  await helper.setConditions({latency: helper.rttMs}, laterSession.protocol);
  await helper.setConditions({uploadThroughput: 100 * 1024});
  await helper.measureNavigation('http://localhost:8000');
  helper.logThrottling('fetch()', await helper.measureFetch(), true);
  await laterSession.disconnect();

  testRunner.completeTest();
})

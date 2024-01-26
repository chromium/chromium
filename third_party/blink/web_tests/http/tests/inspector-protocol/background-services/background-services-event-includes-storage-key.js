async function setUp(dp) {
  // Grant permission to register Background Fetch event.
  await dp.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['backgroundFetch'],
  });

  await dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  dp.BackgroundService.setRecording(
      {shouldRecord: true, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
}

async function tearDown(dp) {
  dp.BackgroundService.setRecording(
      {shouldRecord: false, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startURL(
      'resources/background-services.html',
      `Tests that background service events are reported with storage key.\n`);

  await session.evaluateAsync('installSW()');
  await setUp(dp);

  testRunner.log('Perform background fetch:');
  session.evaluate(
      `sw.backgroundFetch.fetch('my-fetch-success', 'background-fetch.txt')`);
  const eventReceived =
      await dp.BackgroundService.onceBackgroundServiceEventReceived();
  testRunner.log(`background service event received with storage key: ${
      eventReceived.params.backgroundServiceEvent.storageKey ? true : false}`);

  await tearDown(dp);
  testRunner.completeTest();
});

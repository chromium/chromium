async function setUp(dp) {
  // Grant permission to register Background Sync events.
  await dp.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['backgroundSync', 'backgroundFetch'],
  });

  await dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  dp.BackgroundService.setRecording({shouldRecord: true, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();

  await dp.BackgroundService.startObserving({service: 'backgroundSync'});
  dp.BackgroundService.setRecording({shouldRecord: true, service: 'backgroundSync'});
  await dp.BackgroundService.onceRecordingStateChanged();
}

async function tearDown(dp) {
  dp.BackgroundService.setRecording({shouldRecord: false, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
  dp.BackgroundService.setRecording({shouldRecord: false, service: 'backgroundSync'});
  await dp.BackgroundService.onceRecordingStateChanged();
}

async function waitForEvents(dp, numEvents) {
  const events = [];
  for (let i = 0; i < numEvents; i++)
    events.push(await dp.BackgroundService.onceBackgroundServiceEventReceived());
  return events.map(event => event.params.backgroundServiceEvent).map(event => {
    // Remove the `serviceWorkerRegistrationId` property since it can change.
    delete event.serviceWorkerRegistrationId;

    // Sort the metadata.
    event.eventMetadata = event.eventMetadata.sort(
        (m1, m2) => JSON.stringify(m1) < JSON.stringify(m2) ? -1 : 1);

    return event;
  });
}

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/background-services.html',
      `Tests that the expected events are received.`);

  await session.evaluateAsync('installSW()');
  await setUp(dp);

  testRunner.log('Background Fetch Success:');
  session.evaluate(`sw.backgroundFetch.fetch('my-fetch-success', 'background-fetch.txt')`);
  testRunner.log(await waitForEvents(dp, 5));

  testRunner.log('Background Fetch Failure:');
  session.evaluate(`sw.backgroundFetch.fetch('my-fetch-failure', '/missing/')`);
  testRunner.log(await waitForEvents(dp, 5));

  testRunner.log('Background Sync Success:');
  session.evaluate(`sw.sync.register('background-sync-resolve')`);
  testRunner.log(await waitForEvents(dp, 3));

  testRunner.log('Background Sync Failure:');
  session.evaluate(`sw.sync.register('background-sync-reject')`);
  testRunner.log(await waitForEvents(dp, 3));

  await tearDown(dp);
  testRunner.completeTest();
});

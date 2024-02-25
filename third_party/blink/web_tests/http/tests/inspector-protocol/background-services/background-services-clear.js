(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/background-services.html',
      `Tests that background service events are received when appropriate.`);

  await dp.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['backgroundFetch'],
  });

  await session.evaluateAsync('installSW()');

  let receivedEvent = false;
  dp.BackgroundService.onBackgroundServiceEventReceived(() => receivedEvent = true);

  dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
  // Initially there should be zero logged events.
  testRunner.log(`Has events initially: ${receivedEvent}`);

  dp.BackgroundService.setRecording({shouldRecord: true, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
  session.evaluate(`sw.backgroundFetch.fetch('my-fetch-with-recording', '/')`);
  testRunner.log(await session.evaluateAsync('waitForMessageFromSW()'));
  // Events should have been received since `Recording` is on.
  testRunner.log(`Has events with recording on: ${receivedEvent}`);

  // Reset parameters and clear the stored events.
  receivedEvent = false;
  dp.BackgroundService.setRecording({shouldRecord: false, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
  dp.BackgroundService.clearEvents({service: 'backgroundFetch'});
  dp.BackgroundService.stopObserving({service: 'backgroundFetch'});

  // There shouldn't be any events now that they have been cleared.
  await dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  testRunner.log(`Has events initially: ${receivedEvent}`);

  testRunner.completeTest();
});

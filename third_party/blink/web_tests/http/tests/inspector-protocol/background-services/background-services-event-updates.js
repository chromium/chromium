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

  await dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  // Initially there should be zero logged events.
  testRunner.log(`Has events initially: ${receivedEvent}`);

  session.evaluate(`sw.backgroundFetch.fetch('my-fetch-no-recording', '/')`);
  testRunner.log(await session.evaluateAsync('waitForMessageFromSW()'));
  // No events should be received since `Recording` is off.
  testRunner.log(`Has events with recording off: ${receivedEvent}`);

  dp.BackgroundService.setRecording({shouldRecord: true, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
  session.evaluate(`sw.backgroundFetch.fetch('my-fetch-with-recording', '/')`);
  testRunner.log(await session.evaluateAsync('waitForMessageFromSW()'));
  // Events should have been received since `Recording` is on.
  testRunner.log(`Has events with recording on: ${receivedEvent}`);

  // Reset parameters.
  receivedEvent = false;
  dp.BackgroundService.setRecording({shouldRecord: false, service: 'backgroundFetch'});
  await dp.BackgroundService.onceRecordingStateChanged();
  dp.BackgroundService.stopObserving({service: 'backgroundFetch'});

  await dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  // We should receive the logged events initially even with recording off.
  testRunner.log(`Has events initially: ${receivedEvent}`);

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const {page, session, dp} = await testRunner.startBlank(
      `Tests basic function of the BackgroundService domain.`);

  // Enable non-existant service.
  let result = await dp.BackgroundService.startObserving({service: 'fakeservice'});
  testRunner.log(`Found error: ${result.error.message}`);

  // Enable actual service.
  result = await dp.BackgroundService.startObserving({service: 'backgroundFetch'});
  testRunner.log(`Enabled successfully: ${!result.error}`);

  // Enabling a Service should return the recording state.
  dp.BackgroundService.startObserving({service: 'backgroundSync'});
  result = await dp.BackgroundService.onceRecordingStateChanged();
  testRunner.log(result.params);

  // Enable recording.
  dp.BackgroundService.setRecording({shouldRecord: true, service: 'backgroundSync'});
  result = await dp.BackgroundService.onceRecordingStateChanged();
  testRunner.log(result.params);

  // Disable recording.
  dp.BackgroundService.setRecording({shouldRecord: false, service: 'backgroundSync'});
  result = await dp.BackgroundService.onceRecordingStateChanged();
  testRunner.log(result.params);

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that connectTiming is reported on requestWillBeSentExtraInfo.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  const promises = Promise.all([
    dp.Network.onceRequestWillBeSentExtraInfo(),
    dp.Network.onceResponseReceived(),
  ]);

  await session.evaluate(`fetch('index.html');`);

  const [requestWillBeSentExtraInfoEvent, responseReceivedEvent] = await promises;
  const connectTiming = requestWillBeSentExtraInfoEvent.params.connectTiming;

  testRunner.log(`"connectTiming" in event.params: ${"connectTiming" in requestWillBeSentExtraInfoEvent.params}`);
  const requestTiming = responseReceivedEvent.params.response.timing;
  testRunner.log(`connectTiming.requestTime == requestTiming.requestTime: ${connectTiming.requestTime == requestTiming.requestTime}`);
  testRunner.completeTest();
})

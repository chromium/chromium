(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that connectTiming is reported on requestWillBeSentExtraInfo.`);

  await dp.Network.enable();
  testRunner.log('Network Enabled');

  let requestWillBeSentExtraInfoCallback;
  const requestWillBeSentExtraInfoPromise = new Promise(resolve => requestWillBeSentExtraInfoCallback = resolve);
  let connectTiming;

  dp.Network.onRequestWillBeSentExtraInfo(event => {
    testRunner.log(`"connectTiming" in event.params: ${"connectTiming" in event.params}`);
    connectTiming = event.params.connectTiming;
    requestWillBeSentExtraInfoCallback();
  });

  await session.evaluate(`fetch('index.html');`);

  await requestWillBeSentExtraInfoPromise;
  const responseReceived = (await dp.Network.onceResponseReceived()).params;
  const requestTiming = responseReceived.response.timing;
  testRunner.log(`connectTiming.requestTime == requestTiming.requestTime: ${connectTiming.requestTime == requestTiming.requestTime}`);
  testRunner.completeTest();
})

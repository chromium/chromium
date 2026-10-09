(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that Network.requestServedFromCache are reported`);

  await dp.Page.enable();
  await dp.Network.enable();

  const cachedRequestIds = new Set();
  dp.Network.onRequestServedFromCache(event => {
    cachedRequestIds.add(event.params.requestId);
    testRunner.log(event);
  });
  const cachedLoadingFinished = dp.Network.onceLoadingFinished(
      event => cachedRequestIds.has(event.params.requestId));

  let load = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({
    url: testRunner.url('resources/one-script.html')
  });
  await load;

  load = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await load;

  const {encodedDataLength, encodedBodyLength} =
      (await cachedLoadingFinished).params;
  testRunner.log(
      `Cached response transfer size is zero: ${encodedDataLength === 0}`);
  testRunner.log(`Cached response encoded body size is positive: ${
      encodedBodyLength > 0}`);

  testRunner.completeTest();
})

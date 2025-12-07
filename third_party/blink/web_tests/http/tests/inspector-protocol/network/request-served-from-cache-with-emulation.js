(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that Network.requestServedFromCache are reported with device emulation`);

  await dp.Page.enable();
  await dp.Network.enable();
  testRunner.log(
    await dp.Emulation.setDeviceMetricsOverride({
      width: 800,
      height: 600,
      deviceScaleFactor: 1,
      mobile: false,
    })
  );

  dp.Network.onRequestServedFromCache(event => {
    testRunner.log(event);
  });

  let load = dp.Page.onceLoadEventFired();
  await dp.Page.navigate({
    url: testRunner.url('resources/one-script.html')
  });
  await load;

  load = dp.Page.onceLoadEventFired();
  await dp.Page.reload();
  await load;

  testRunner.completeTest();
})

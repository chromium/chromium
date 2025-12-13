(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that Network.requestServedFromCache are reported`);

  await dp.Page.enable();
  await dp.Network.enable();

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

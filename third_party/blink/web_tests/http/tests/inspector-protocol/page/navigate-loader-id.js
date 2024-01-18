(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.navigate() returns the loader id that matches one in network events`);

  await dp.Page.enable();
  await dp.Network.enable();
  var navigatePromise = dp.Page.navigate({url: testRunner.url('../resources/blank.html')});
  var requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
  var navigateResult = (await  navigatePromise).result;

  if (navigateResult.loaderId !== requestWillBeSent.loaderId) {
    testRunner.fail(`loaderId from Page.navigate is ${navigateResult.loaderId}
      and loaderId in RequestWillBeSent is ${requestWillBeSent.loaderId}`);
  }
  testRunner.completeTest();
})

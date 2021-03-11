(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that calling Page.navigate while a navigation is running cancels the previous navigation`);

  await dp.Page.enable();
  const navigatePromise1 = dp.Page.navigate({ url: testRunner.url('../resources/test-page.html') });
  const navigatePromise2 = dp.Page.navigate({ url: testRunner.url('../resources/image.html') });

  testRunner.log(await navigatePromise1);
  testRunner.log(await navigatePromise2);

  await dp.Fetch.enable();
  const navigatePromise3 = dp.Page.navigate({ url: testRunner.url('../resources/test-page.html')});
  await dp.Fetch.onceRequestPaused();
  const navigatePromise4 = dp.Page.navigate({ url: testRunner.url('../resources/final.html')});
  await dp.Fetch.disable();

  testRunner.log(await navigatePromise3);
  testRunner.log(await navigatePromise4);

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that calling Page.navigate while a navigation is running cancels the previous navigation`);

  // Call Page.navigate() consecutively (start second navigation before first
  // one has started).
  await dp.Page.enable();
  const navigatePromise1 = dp.Page.navigate({ url: testRunner.url('../resources/test-page.html') });
  const navigatePromise2 = dp.Page.navigate({ url: testRunner.url('../resources/image.html') });
  const loadPromise = dp.Page.onceLoadEventFired();

  testRunner.log(await navigatePromise1);
  testRunner.log(await navigatePromise2);

  // Wait for navigation to finish in renderer.
  await loadPromise;

  // Call Page.navigate() consecutively (start second navigation after first
  // one has started and has sent a network request).
  await dp.Fetch.enable();
  const navigatePromise3 = dp.Page.navigate({ url: testRunner.url('../resources/test-page.html')});
  await dp.Fetch.onceRequestPaused();
  const navigatePromise4 = dp.Page.navigate({ url: testRunner.url('../resources/final.html')});
  await dp.Fetch.disable();

  testRunner.log(await navigatePromise3);
  testRunner.log(await navigatePromise4);

  testRunner.completeTest();
})

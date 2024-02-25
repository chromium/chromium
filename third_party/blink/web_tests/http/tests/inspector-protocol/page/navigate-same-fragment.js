(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that Page.navigate triggers Page.navigatedWithinDocument when navigating to the same fragment`);
  await dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html#foo') });
  await dp.Page.onceNavigatedWithinDocument();
  testRunner.log('Page.navigatedWithinDocument #1');
  // Top-level client redirect to the same fragment URL should not be treated as a reload.
  dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html#foo') });
  await dp.Page.onceNavigatedWithinDocument();
  testRunner.log('Page.navigatedWithinDocument #2');
  testRunner.completeTest();
});

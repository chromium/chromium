(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that Page.navigate within same document does not report loaderId`);

  await dp.Page.enable();
  const result = (await dp.Page.navigate({url: testRunner.url('../resources/inspector-protocol-page.html#foo')})).result;
  // Assure the above is same-document navigation.
  await dp.Page.onceNavigatedWithinDocument();
  testRunner.log(`loaderId (undefined expected): ${result.loaderId}`);
  testRunner.completeTest();
});

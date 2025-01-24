(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startBlank(
      `Tests that Page.getResourceContent returns an error when no-store header is set`);

  await dp.Page.enable();
  const navigated = dp.Page.onceFrameNavigated();
  await session.navigate(testRunner.url('./resources/cache-no-store-page.php'));
  const frameId = (await navigated).params.frame.id

  const response = await dp.Page.getResourceContent({frameId, url: testRunner.url('./resources/cache-no-store-page.php')});

  testRunner.log(response.error || 'FAIL: error not reported');
  testRunner.completeTest();
})

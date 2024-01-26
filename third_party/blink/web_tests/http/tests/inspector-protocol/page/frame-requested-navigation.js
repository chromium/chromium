(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests reasons for client-requested navigation are properly reported.');

  await dp.Page.enable();
  dp.Page.navigate({url: testRunner.url('resources/frame-requested-navigation-1.html')});
  for (let i = 0; i < 6; ++i) {
    const params = (await dp.Page.onceFrameRequestedNavigation()).params;
    testRunner.log(`Frame requested navigation to ${params.url}: ${params.reason}`);
  }
  testRunner.completeTest();
})

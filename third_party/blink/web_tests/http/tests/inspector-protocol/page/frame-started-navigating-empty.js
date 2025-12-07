(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests frameStartedNavigating events when navigation is initiated by cdp command');

  await dp.Page.enable();

  dp.Page.onFrameStartedNavigating(event => {
    testRunner.log(event, `frameStartedNavigating`, testRunner.stabilizeNames,
        ['loaderId']);
  });

  testRunner.log("\nNavigation");
  dp.Page.navigate({url: testRunner.url('../resources/empty.html')});
  await dp.Page.onceFrameStoppedLoading();

  dp.Page.navigate({url: testRunner.url('../resources/empty.html?1')});
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log("\nFragment navigation");
  const current_url = (await dp.Runtime.evaluate(
      {expression: "location.href"})).result.result.value;
  dp.Page.navigate({url: `${current_url}#some_fragment`});
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log("\nReload");
  dp.Page.reload();
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log("\nReload ignoring cache");
  dp.Page.reload({ignoreCache: true});
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log("\nHistory traversal");
  const navigationHistory = (await dp.Page.getNavigationHistory()).result.entries;

  // Go back and forth in the history.
  const historyEntries = navigationHistory.slice(navigationHistory.length - 3);
  for (const historyItem of historyEntries) {
    dp.Page.navigateToHistoryEntry({entryId: historyItem.id});
    await dp.Page.onceFrameStoppedLoading();
  }

  testRunner.completeTest();
})

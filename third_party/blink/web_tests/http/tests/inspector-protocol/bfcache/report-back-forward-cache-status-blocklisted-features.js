(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that back/forward navigations report the bfcache status`);

  await dp.Page.enable();

  // Navigate to Page A.
  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/resources/empty.html');

  // Use a blocklisted feature (Idle detector).
  await session.evaluate(`new Promise(async resolve => {
    let idleDetector = new IdleDetector();
    idleDetector.start();
    resolve();
  });`);

  // Navigate to Page B.
  await page.navigate('resources/empty.html');

  const {result: history} = await dp.Page.getNavigationHistory();

  // Enable for printing the history.
  // testRunner.log(history);

  // Do a 'back' navigation.
  const previous = history.entries[history.currentIndex - 1];
  await historyNavigate(previous.id);

  // Do a 'forward' navigation.
  const current = history.entries[history.currentIndex];
  await historyNavigate(current.id);

  testRunner.completeTest();

  async function historyNavigate(entryId) {
    const statusReportPromise = dp.Page.onceBackForwardCacheNotUsed();
    await dp.Page.navigateToHistoryEntry({entryId});
    const {params: statusReport} = await statusReportPromise;
    testRunner.log(statusReport);
  }
});

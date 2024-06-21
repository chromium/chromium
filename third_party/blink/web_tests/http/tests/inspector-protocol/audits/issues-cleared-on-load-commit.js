(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that inspector issues do not persist through reloads.\n`);

  // Trigger a known issue, inspector-protocol/network/same-site-issue-blocked-cookie-not-secure.js ensures this
  // issue is actually triggered.
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
  + encodeURIComponent('name=value; SameSite=None');
  await session.evaluateAsync(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);

  // Reloading the page should clear the issue.
  await dp.Page.enable();
  await dp.Network.enable();
  dp.Page.reload();
  await Promise.all([dp.Page.onceFrameNavigated(), dp.Network.oncePolicyUpdated()]);

  // This should never be printed.
  dp.Audits.onIssueAdded(() => testRunner.log(`Issue should have been cleared by the reload`));
  await dp.Audits.enable();

  testRunner.completeTest();
})

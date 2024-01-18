(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that setting a cookie with SameSite=None and without Secure triggers an inspector issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  const issuePromise = dp.Audits.onceIssueAdded();
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=None');
  await session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);
  const issue = await issuePromise;
  testRunner.log(issue.params, 'Inspector issue:');

  testRunner.completeTest();
})

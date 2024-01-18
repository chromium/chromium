(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that a subresource accessing a SameSite=Strict cookie across schemes triggers a context downgrade inspector issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  await session.navigate('http://cookie.test:8000/inspector-protocol/resources/empty.html');

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Strict');
  session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);
  const issue = await dp.Audits.onceIssueAdded();
  testRunner.log(issue.params, 'Inspector issue:');

  testRunner.completeTest();
});

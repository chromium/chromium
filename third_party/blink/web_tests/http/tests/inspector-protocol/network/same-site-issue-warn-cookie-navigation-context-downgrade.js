(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that accessing a SameSite=Strict cookie across schemes triggers a context downgrade inspector issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const setCookieUrl = 'http://cookie.test:8000/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Strict');
  const baseURL = 'inspector-protocol/network/resources/hello-world.html'
  const secureUrl = 'https://cookie.test:8443/' + baseURL;

  // Set a SameSite=Strict cookie on the cookie.test domain via an insecure URL.
  let issuePromise = dp.Audits.onceIssueAdded();
  await session.navigate(setCookieUrl);
  // Ignore the quirks mode issue
  await issuePromise;

  issuePromise = dp.Audits.onceIssueAdded();
  // Now navigate to the secure site, this should trigger the issue.
  await helper.jsNavigateWithExtraInfo(secureUrl);

  const issue = await issuePromise;
  testRunner.log(issue.params, 'Inspector issue:');

  testRunner.completeTest();
});

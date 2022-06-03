(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that accessing a SameSite=Strict cookie across schemes triggers a context downgrade inspector issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const setCookieUrl = 'http://cookie.test:8000/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Strict');
  const baseURL = 'inspector-protocol/network/resources/hello-world.html'
  const insecureUrl = 'http://cookie.test:8000/' + baseURL;
  const secureUrl = 'https://cookie.test:8443/' + baseURL;

  // Set a SameSite=Strict cookie on the cookie.test domain
  await session.navigate(setCookieUrl);

  // Navigate first to an insecure site. Note: This isn't strictly necessary
  // because the setCookieUrl is also insecure but this helps to illustrate the
  // point as insecureUrl and secureUrl are the same URL except for the scheme.
  await helper.navigateWithExtraInfo(insecureUrl);
  const issuePromise = dp.Audits.onceIssueAdded();
  // Now navigate to the secure site, this should trigger the issue.
  await helper.jsNavigateWithExtraInfo(secureUrl);

  const issue = await issuePromise;
  testRunner.log(issue.params, 'Inspector issue:');

  testRunner.completeTest();
});

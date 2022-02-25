(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that making cross-party requests which set SameParty cookies sends as a SameSite issue\n`);
  await dp.Network.enable();
  await dp.Audits.enable();

  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=None; SameParty; Secure');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // make a cross-party request to set the cookie, see that it is blocked with enabled first party sets.
  const issuePromises = [dp.Audits.onceIssueAdded(), dp.Audits.onceIssueAdded()];
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  let {responseExtraInfo} = await helper.fetchWithExtraInfo(setCookieUrl);

  // Only show cookie issues
  const issues = await Promise.all(issuePromises);
  const cookieIssues = issues.filter(issue => issue.params.issue.code === 'CookieIssue');

  testRunner.log(responseExtraInfo.params.blockedCookies, 'Javascript initiated subresource blocked set-cookies:');
  testRunner.log(cookieIssues[0].params.issue, "Issue reported: ", ['requestId']);

  testRunner.completeTest();
})

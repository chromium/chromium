(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that making cross-party requests with SameParty cookies results in a blocked cookie issue\n`);
  await dp.Audits.enable();
  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameParty; SameSite=None; Secure');
  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set the SameParty cookie
  await helper.navigateWithExtraInfo(setCookieUrl);

  // navigate away and make a subresource request from javascript, see that the cookie is blocked with enabled first partys sets.
  const issuePromises = [dp.Audits.onceIssueAdded(), dp.Audits.onceIssueAdded()];
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  ({requestExtraInfo} = await helper.fetchWithExtraInfo(firstPartyUrl));

  const issues = await Promise.all(issuePromises);
  const cookieIssues = issues.filter(issue => issue.params.issue.code === 'CookieIssue');

  testRunner.log(requestExtraInfo.params.associatedCookies, 'Javascript initiated subresource associated cookies:');
  testRunner.log(cookieIssues[0].params.issue, "Issue reported: ", ['requestId']);

  testRunner.completeTest();
})

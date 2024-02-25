(async testRunner => {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that third-party cookie that is blocked by third-party phaseout files an issue.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  // Set the cookie. Make sure the cookie won't be excluded by other reasons
  // other than EXCLUDE_THIRD_PARTY_PHASEOUT.
  const response = await dp.Network.setCookie({
    url: 'https://example.test:8443',
    secure: true,
    name: 'foo',
    value: 'bar',
    sameSite: 'None',
    sourcePort: 8443,
  });

  if (response.error)
    testRunner.log(`setCookie failed: ${response.error.message}`);

  // Push events to arrays to prevent async races from causing flakes.
  const requestWillBeSentExtraInfos = [];
  let issueAdded;

  const expectedRequests =
      new Promise(resolve => dp.Network.onRequestWillBeSentExtraInfo(event => {
        requestWillBeSentExtraInfos.push(event.params);
        // There will be the top-level navigation and iframe navigation
        if (requestWillBeSentExtraInfos.length === 2) {
          resolve();
        }
      }));

  const expectedIssue = dp.Audits.onceIssueAdded(event => {
    // Safely ignore irrelevant issue...
    return event.params.issue.code !== 'QuirksModeIssue';
  });

  page.navigate(
      'https://firstparty.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent.php');

  await Promise.all([expectedRequests, expectedIssue]);

  for (const params of requestWillBeSentExtraInfos) {
    testRunner.log(`Number of cookies: ${params.associatedCookies.length}`);
    for (const cookie of params.associatedCookies) {
      testRunner.log(`Cookie blocked: ${!!cookie.blockedReasons.length}`);
    }
  }

  testRunner.log(`Issues:`)
  issueAdded = await expectedIssue;
  testRunner.log(issueAdded.params);

  testRunner.completeTest();
});

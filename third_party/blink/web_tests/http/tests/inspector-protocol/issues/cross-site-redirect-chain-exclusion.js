(async testRunner => {
  // This test requires kCookieSameSiteConsidersRedirectChain to be enabled in order to pass.
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that after a cross-site redirect SameSite cookies file an Issue\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  // Set the cookie.
  const response = await dp.Network.setCookie({
    url: 'https://firstparty.test:8443',
    secure: true,
    name: 'fooStrict',
    value: 'bar',
    sameSite: 'Strict',
  });

  if (response.error)
    testRunner.log(`setCookie failed: ${response.error.message}`);

  // Push events to arrays to prevent async races from causing flakes.
  const requestWillBeSentExtraInfos = [];
  let issueAdded;

  const expectedRequests =
      new Promise(resolve => dp.Network.onRequestWillBeSentExtraInfo(event => {
        requestWillBeSentExtraInfos.push(event.params);
        // There will be the first navigation -> redirect -> final navigation == 3
        if (requestWillBeSentExtraInfos.length === 3) {
          resolve();
        }
      }));

  const expectedIssue = dp.Audits.onceIssueAdded(event => {
    // Safely ignore irrelevant issue...
    return event.params.issue.code !== 'QuirksModeIssue';
  });

  page.navigate(
      'https://firstparty.test:8443/inspector-protocol/resources/redirect-chain.html?start');

  await expectedRequests;

  issueAdded = await expectedIssue;
  testRunner.log(issueAdded.params);

  testRunner.completeTest();
});

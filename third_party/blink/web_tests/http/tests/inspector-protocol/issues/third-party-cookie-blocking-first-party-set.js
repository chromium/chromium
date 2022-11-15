(async testRunner => {
  const {page, session, dp} = await testRunner.startBlank(
      'Test third-party cookie blocking between sites in same First-Party Set');

  testRunner.log('Test started');
  await dp.Network.enable();
  await dp.Audits.enable();

  testRunner.runTestSuite([
    // Without third-party cookie blocking.
    async function thirdPartyCookieBlockingDisabled() {
      // Set the cookie.
      const response = await dp.Network.setCookie({
        url: 'https://cookie.test:8443',
        secure: true,
        name: 'foo',
        value: 'bar',
        sameSite: 'None',
      });
      if (response.error)
        testRunner.log(`setCookie failed: ${response.error.message}`);

      const handleRequestWillBeSent = event => {
        testRunner.log(`Number of cookies: ${event.params.associatedCookies.length}`);
        for (const cookie of event.params.associatedCookies) {
          testRunner.log(`Cookie not blocked: ${!cookie.blockedReasons.length}`);
        }
      };
      await dp.Network.onRequestWillBeSentExtraInfo(handleRequestWillBeSent);

      await page.navigate('https://firstparty.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent-first-party-set.php');

      await dp.Network.offRequestWillBeSentExtraInfo(handleRequestWillBeSent);
    },
    // With third-party cookie blocking.
    async function thirdPartyCookieBlockingEnabled() {
      await session.evaluate('testRunner.setBlockThirdPartyCookies(true)');

      // Push events to arrays to prevent async races from causing flakes.
      const requestWillBeSentExtraInfos = [];
      const issueAddeds = [];

      const handleRequestWillBeSent = event => {
        requestWillBeSentExtraInfos.push(event.params);
      };
      await dp.Network.onRequestWillBeSentExtraInfo(handleRequestWillBeSent);

      const handleIssueAdded = event => {
        // Safely ignore irrelevant issue...
        if (event.params.issue.code === 'QuirksModeIssue') return;
        issueAddeds.push(event.params);
      };
      await dp.Audits.onIssueAdded(handleIssueAdded);

      await page.navigate('https://firstparty.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent-first-party-set.php');

      await dp.Network.offRequestWillBeSentExtraInfo(handleRequestWillBeSent);
      await dp.Audits.offIssueAdded(handleIssueAdded);

      for (const params of requestWillBeSentExtraInfos) {
        testRunner.log(`Number of cookies: ${params.associatedCookies.length}`);
        for (const cookie of params.associatedCookies) {
          testRunner.log(`Cookie blocked: ${!!cookie.blockedReasons.length}`);
          testRunner.log(`Reasons: ${cookie.blockedReasons}`);
        }
      }

      for (const params of issueAddeds) {
        testRunner.log(params);
      }
    },
  ]);
});

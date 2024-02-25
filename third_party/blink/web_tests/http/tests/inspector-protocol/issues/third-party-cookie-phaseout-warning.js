(async testRunner => {
  const {page, session, dp} = await testRunner.startBlank(
     `Verifies that third-party cookie that will be affected by third-party phaseout files an issue.\n`);

    await dp.Network.enable();
    await dp.Audits.enable();

    // Set the cookie. Make sure the cookie won't be excluded by other reasons
    // to be able to have the WARN_THIRD_PARTY_PHASEOUT reason.
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
    const issueAddeds = [];

    // There will be the top-level navigation and iframe navigation
    let expectedExtraInfoCalls = 2;
    let handledRequestResolve;
    const handledRequest = new Promise(resolve => {
      handledRequestResolve = resolve;
    });

    const handleRequestWillBeSent = event => {
      requestWillBeSentExtraInfos.push(event.params);
      if (--expectedExtraInfoCalls === 0) {
        handledRequestResolve();
      }
    };

    dp.Network.onRequestWillBeSentExtraInfo(handleRequestWillBeSent);

    const handleIssueAdded = event => {
      // Safely ignore irrelevant issue...
      if (event.params.issue.code === 'QuirksModeIssue') return;
      issueAddeds.push(event.params);
    };

    dp.Audits.onIssueAdded(handleIssueAdded);

    await page.navigate('https://firstparty.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent.php');

    await handledRequest;

    dp.Network.offRequestWillBeSentExtraInfo(handleRequestWillBeSent);
    dp.Audits.offIssueAdded(handleIssueAdded);

    for (const params of requestWillBeSentExtraInfos) {
      testRunner.log(`Number of cookies: ${params.associatedCookies.length}`);
      for (const cookie of params.associatedCookies) {
        testRunner.log(`Cookie blocked: ${!!cookie.blockedReasons.length}`);
      }
    }

    testRunner.log(`Issues:`)
    for (const params of issueAddeds) {
      testRunner.log(params);
    }

    testRunner.completeTest();
  });

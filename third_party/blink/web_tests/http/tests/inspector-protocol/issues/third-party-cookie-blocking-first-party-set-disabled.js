(async testRunner => {
  const {page, dp} = await testRunner.startBlank(
      'Test third-party cookie blocking disabled between sites in same First-Party Set');

  testRunner.log('Test started');
  await dp.Network.enable();
  await dp.Audits.enable();

  // Without third-party cookie blocking.
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

  let expectedExtraInfoCalls = 2;
  let handledRequestResolve;
  const handledRequest = new Promise(resolve => {
    handledRequestResolve = resolve;
  });

  const handleRequestWillBeSent = event => {
    testRunner.log(`Number of cookies: ${event.params.associatedCookies.length}`);
    for (const cookie of event.params.associatedCookies) {
      testRunner.log(`Cookie not blocked: ${!cookie.blockedReasons.length}`);
    }
    if (--expectedExtraInfoCalls === 0) {
      handledRequestResolve();
    }
  };

  dp.Network.onRequestWillBeSentExtraInfo(handleRequestWillBeSent);

  await page.navigate('https://firstparty.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent-first-party-set.php');

  await handledRequest;

  dp.Network.offRequestWillBeSentExtraInfo(handleRequestWillBeSent);;

  testRunner.completeTest();
 });

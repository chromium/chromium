(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Verifies that blocked response cookies with non-positive Max-Age can be reported.');
  await dp.Network.enable();
  await dp.Network.setCacheDisabled({cacheDisabled: true});

  const helper =
      (await testRunner.loadScript('resources/extra-info-helper.js'))(dp,
                                                                      session);
  await helper.navigateWithExtraInfo(
      'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html');

  // Keep a finite-expiry control and cover zero and negative Max-Age.
  const cookieUrl =
      'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie=';
  for (const maxAge of [60, 0, -1]) {
    const cookieLine =
        `expiry_probe=1; Max-Age=${maxAge}; SameSite=Strict; Path=/`;
    const {responseExtraInfo} = await helper.fetchWithExtraInfo(
        cookieUrl + encodeURIComponent(cookieLine));
    const blockedCookies = responseExtraInfo.params.blockedCookies;
    testRunner.log(`Max-Age=${maxAge}:`);
    testRunner.log(`Blocked cookie count: ${blockedCookies.length}`);
    const {blockedReasons, cookie} = blockedCookies[0];
    testRunner.log(blockedReasons, 'Blocked reasons: ');
    testRunner.log(`Cookie name: ${cookie.name}`);
    testRunner.log(`Session: ${cookie.session}`);
    if (maxAge > 0) {
      const validExpiry = Number.isFinite(cookie.expires) && cookie.expires > 0;
      testRunner.log(`Positive finite expiry: ${validExpiry}`);
    } else {
      testRunner.log(`Expiry is null: ${cookie.expires === null}`);
    }
  }

  const {result: {cookies}} = await dp.Network.getCookies({urls: [cookieUrl]});
  const storedCount =
      cookies.filter(cookie => cookie.name === 'expiry_probe').length;
  testRunner.log(`Stored test cookie count: ${storedCount}`);
  testRunner.completeTest();
})

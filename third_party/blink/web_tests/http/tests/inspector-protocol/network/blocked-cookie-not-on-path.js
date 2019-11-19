(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that cookies not within the current path of the request to a domain send Network.*ExtraInfo events with corresponding blocked cookies.\n`);
  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; Path=/inspector-protocol/network/resources/set-cookie.php');
  const differentPathUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.php';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set a cookie with a path
  await helper.navigateWithExtraInfo(setCookieUrl);

  // navigate to a different path to see that the cookie was blocked
  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(differentPathUrl);
  testRunner.log(`requestWillBeSentExtraInfo blocked cookies: ${JSON.stringify(requestExtraInfo.params.blockedCookies, null, 2)}`);

  testRunner.completeTest();
})

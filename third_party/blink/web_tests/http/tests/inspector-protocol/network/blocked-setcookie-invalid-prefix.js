(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that setting a cookie with an invalid __Secure- or __Host- prefix sends us Network.ResponseReceivedExtraInfo events with corresponding blocked cookies.\n`);
  await dp.Network.enable();

  const setCookieUrlBadSecure = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('__Secure-name=value');
  const setCookieUrlBadHost = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('__Host-name=value; Secure');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  var {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrlBadSecure);
  testRunner.log(`Bad __Secure- prefix blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}`);

  var {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrlBadHost);
  testRunner.log(`Bad __Host- prefix blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}`);

  testRunner.completeTest();
})

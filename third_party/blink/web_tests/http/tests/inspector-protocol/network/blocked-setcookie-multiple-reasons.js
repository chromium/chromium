(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that we get multiple cookie blocked reasons when overwriting a Secure cookie over an insecure connection.`);
  await dp.Network.enable();

  const setCookieSecure = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie-secure.php';
  const setCookieInsecure = 'http://cookie.test:8000/inspector-protocol/network/resources/set-cookie-secure.php';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set a secure cookie
  await helper.navigateWithExtraInfo(setCookieSecure);

  // try to overwrite it with an insecure cookie
  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieInsecure);
  testRunner.log(`set-cookie that would overwrite secure cookie blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}`);

  testRunner.completeTest();
})

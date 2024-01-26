(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that when we get a non-secure set-cookie header that would overwrite a secure one, we get a Network.ResponseReceivedExtraInfo event with the blocked cookie.`);
  await dp.Network.enable();

  const setCookieSecure = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie-secure.php';
  const setCookieInsecure = 'http://cookie.test:8000/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set a secure cookie
  await helper.navigateWithExtraInfo(setCookieSecure);

  // try to overwrite it with an insecure cookie
  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieInsecure);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'set-cookie that would overwrite secure cookie blocked set-cookies:');

  testRunner.completeTest();
})

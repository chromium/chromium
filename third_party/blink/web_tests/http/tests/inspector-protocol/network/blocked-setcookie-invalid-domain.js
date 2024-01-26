(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that setting a cookie with a domain attribute which does not match the current domain sends a Network.ResponseReceivedExtraInfo event with the corresponding blocked cookie.\n`);
  await dp.Network.enable();

  const setCookieUrlBadDomain = 'https://thirdparty.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; Domain=cookie.test');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // try to set the cookie from a different domain than the one it specifies, see that it is blocked
  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrlBadDomain);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Bad domain attribute blocked set-cookies:');

  testRunner.completeTest();
})

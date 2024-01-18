(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that receiving a set-cookie header with only whitespace sends a Network.ResponseReceivedExtraInfo event with the blocked cookie.\n`);
  await dp.Network.enable();

  const setCookieInvalidSyntax = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie-no-cookie-content.php';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieInvalidSyntax);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'NoCookieContent blocked set-cookies:');

  testRunner.completeTest();
})

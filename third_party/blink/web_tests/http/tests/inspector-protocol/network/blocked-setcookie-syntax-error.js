(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that receiving a set-cookie header with invalid syntax sends a Network.ResponseReceivedExtraInfo event with the blocked cookie.\n`);
  await dp.Network.enable();

  const setCookieInvalidSyntax = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie-invalid-syntax.php';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieInvalidSyntax);
  testRunner.log(`Invalid syntax blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}`);

  testRunner.completeTest();
})

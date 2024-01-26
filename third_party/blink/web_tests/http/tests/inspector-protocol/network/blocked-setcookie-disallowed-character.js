(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that receiving a set-cookie header with a disallowed character sends a Network.ResponseReceivedExtraInfo event with the blocked cookie.\n`);
  await dp.Network.enable();

  const setCookieDisallowedCharacter = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie-disallowed-character.php';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieDisallowedCharacter);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Disallowed character blocked set-cookies:');

  testRunner.completeTest();
})

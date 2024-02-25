(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that receiving a set-cookie header where the name + value size exceeds the max size causes a Network.ResponseReceivedExtraInfo event to be sent containing the correct blocked cookie reason.\n`);
  await dp.Network.enable();

  const setCookieNameValuePairExceedsMaxSize = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie-name-value-pair-exceeds-max-size.php';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieNameValuePairExceedsMaxSize);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Invalid name/value pair size blocked set-cookies:');

  testRunner.completeTest();
})

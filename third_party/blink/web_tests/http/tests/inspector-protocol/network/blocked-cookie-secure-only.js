(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that storing and sending Secure cookies over http sends Network.*ExtraInfo events with corresponding blocked cookies.\n`);
  await dp.Network.enable();

  const setCookieInsecureUrl = 'http://cookie.test:8000/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; Secure');
  const setCookieSecureUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; Secure');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // navigate to the set-cookie over http and see that the cookie gets blocked
  var {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieInsecureUrl);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'ResponseReceivedExtraInfo blocked cookies:');

  // navigate to the set-cookie over https to actually set the cookie
  await helper.navigateWithExtraInfo(setCookieSecureUrl);

  // navigate there again over http to see that the cookie was not sent
  var {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieInsecureUrl);
  testRunner.log(requestExtraInfo.params.associatedCookies, 'RequestWillBeSentExtraInfo blocked cookies:');

  testRunner.completeTest();
})

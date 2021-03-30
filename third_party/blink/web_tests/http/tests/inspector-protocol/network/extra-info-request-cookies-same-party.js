(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that Network.RequestWillBeSentExtraInfo events report structured request cookies with the correct SameParty attribute.\n`);
  await dp.Network.enable();
  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name1=value1;SameParty;Secure;Domain=cookie.test');

  // Set cookies in a domain.
  await helper.navigateWithExtraInfo(setCookieUrl);

  // Navigate to a the same domain to see that the cookie is reported.
  const sendCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const response = await helper.navigateWithExtraInfo(sendCookieUrl);
  testRunner.log(response.requestExtraInfo.params.associatedCookies, 'requestWillBeSentExtraInfo request cookies on same domain:');
  testRunner.completeTest();
})

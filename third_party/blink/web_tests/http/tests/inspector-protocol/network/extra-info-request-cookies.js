(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that Network.RequestWillBeSentExtraInfo events report structured request cookies.\n`);
  await dp.Network.enable();
  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const setCookieUrl1 = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name1=value1;SameSite=None;Secure;Domain=cookie.test');
  const setCookieUrl2 = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name2=value2');

  // Set cookies in a domain.
  await helper.navigateWithExtraInfo(setCookieUrl1);
  await helper.navigateWithExtraInfo(setCookieUrl2);

  // Navigate to a the same domain to see that the cookies are reported.
  const sendCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const response1 = await helper.navigateWithExtraInfo(sendCookieUrl);
  testRunner.log(response1.requestExtraInfo.params.associatedCookies, 'requestWillBeSentExtraInfo request cookies on same domain:');

  // Navigate to a subdomain to see that only the cookies are reported (one as blocked now).
  const subdomainUrl = 'https://subdomain.cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const response2 = await helper.navigateWithExtraInfo(subdomainUrl);
  testRunner.log(response2.requestExtraInfo.params.associatedCookies, 'requestWillBeSentExtraInfo request cookies on subdomain');

  testRunner.completeTest();
})

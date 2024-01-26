(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that cookies blocked for a request of a subdomain of the cookie's domain are included in the blocked cookies of Network.RequestWillBeSentExtraInfo events.\n`);
  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value');
  const subdomainUrl = 'https://subdomain.cookie.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set a cookie in a domain
  await helper.navigateWithExtraInfo(setCookieUrl);

  // navigate to a subdomain to see that the cookie was blocked
  const {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(subdomainUrl);
  testRunner.log(requestExtraInfo.params.associatedCookies, 'requestWillBeSentExtraInfo blocked cookies:');

  testRunner.completeTest();
})

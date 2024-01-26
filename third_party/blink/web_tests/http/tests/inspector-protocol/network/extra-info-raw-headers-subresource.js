(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that cross origin requests have their raw headers shown in Network.*ExtraInfo events by checking cookie headers.\n`);

  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; Secure; SameSite=None; HttpOnly');
  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // navigate to a third party domain
  await helper.navigateWithExtraInfo(thirdPartyUrl);

  // set the cookie with a subresource request
  const {responseExtraInfo} = await helper.fetchWithExtraInfo(setCookieUrl);
  testRunner.log(`Response set-cookie header: ${responseExtraInfo.params.headers['set-cookie']}\n`);

  // make a cross origin subresource request to the domain with the cookie
  const {requestExtraInfo} = await helper.fetchWithExtraInfo(firstPartyUrl);
  testRunner.log(`Request cookie header: ${requestExtraInfo.params.headers['Cookie']}`);

  testRunner.completeTest();
})

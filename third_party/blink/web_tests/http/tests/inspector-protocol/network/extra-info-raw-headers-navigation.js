(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that navigation requests have their raw headers shown in Network.*ExtraInfo events by checking cookie headers.\n`);

  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; Secure; SameSite=None; HttpOnly');
  const helloWorldUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const otherDomainUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set the cookie with a navigation request
  const {responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrl);
  testRunner.log(`Response set-cookie header: ${responseExtraInfo.params.headers['set-cookie']}\n`);

  // navigate to a different domain
  await helper.navigateWithExtraInfo(otherDomainUrl);

  // make a cross origin navigation request to the domain with the cookie
  const {requestExtraInfo} = await helper.navigateWithExtraInfo(helloWorldUrl);
  testRunner.log(`Request cookie header: ${requestExtraInfo.params.headers['Cookie']}`);

  testRunner.completeTest();
})

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that making navigation requests from browser which set nonsecure SameParty cookies send us Network.ResponseReceivedExtraInfo events with corresponding blocked set-cookies.\n`);
  await dp.Network.enable();

  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=None; SameParty');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // make a cross-party navigation via browser to set the cookie, see that it is blocked
  await helper.navigateWithExtraInfo(firstPartyUrl);
  ({responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrl));
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Browser initiated navigation blocked set-cookies:');

  testRunner.completeTest();
})

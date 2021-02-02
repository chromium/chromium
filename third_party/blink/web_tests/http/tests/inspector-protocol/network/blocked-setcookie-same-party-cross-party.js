(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that making cross-party requests which set SameParty cookies send us Network.ResponseReceivedExtraInfo events with corresponding blocked set-cookies.\n`);
  await dp.Network.enable();

  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=None; SameParty; Secure');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // make a cross-party request to set the cookie, see that it is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  let {responseExtraInfo} = await helper.fetchWithExtraInfo(setCookieUrl);
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Javascript initiated subresource blocked set-cookies:');

  // make a cross-party navigation via javascript to set the cookie, see that it is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  ({responseExtraInfo} = await helper.jsNavigateWithExtraInfo(setCookieUrl));
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Javascript initiated navigation blocked set-cookies:');

  // make a cross-party navigation via browser to set the cookie, see that it is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  ({responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrl));
  testRunner.log(responseExtraInfo.params.blockedCookies, 'Browser initiated navigation blocked set-cookies:');

  testRunner.completeTest();
})

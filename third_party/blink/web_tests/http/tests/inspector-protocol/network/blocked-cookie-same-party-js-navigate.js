(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that making cross-party navigation requests from JS with SameParty cookies sends us Network.RequestWillBeSentExtraInfo events with corresponding associated cookies.\n`);
  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameParty; SameSite=None; Secure');
  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set the SameParty cookie
  await helper.navigateWithExtraInfo(setCookieUrl);

  // navigate to a cross-party domain and back from javascript and see that the cookie is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  ({requestExtraInfo} = await helper.jsNavigateWithExtraInfo(firstPartyUrl));
  testRunner.log(requestExtraInfo.params.associatedCookies, 'Javascript initiated navigation associated cookies:');

  testRunner.completeTest();
})

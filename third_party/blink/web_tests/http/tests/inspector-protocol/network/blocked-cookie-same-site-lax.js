(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that making cross origin requests with SameSite=Lax cookies sends us Network.RequestWillBeSentExtraInfo events with corresponding blocked cookies.\n`);
  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Lax');
  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set the SameSite=Lax cookie
  await helper.navigateWithExtraInfo(setCookieUrl);

  // navigate to a different domain and back from the browser and see that the cookie is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {requestExtraInfo} = await helper.navigateWithExtraInfo(firstPartyUrl);
  testRunner.log(`Browser initiated navigation blocked cookies: ${JSON.stringify(requestExtraInfo.params.blockedCookies, null, 2)}\n`);

  // navigate to a different domain and back from javascript and see that the cookie is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {requestExtraInfo} = await helper.jsNavigateWithExtraInfo(firstPartyUrl);
  testRunner.log(`Javascript initiated navigation blocked cookies: ${JSON.stringify(requestExtraInfo.params.blockedCookies, null, 2)}\n`);

  // navigate away and make a subresource request from javascript, see that the cookie is blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {requestExtraInfo} = await helper.fetchWithExtraInfo(firstPartyUrl);
  testRunner.log(`Javascript initiated subresource blocked cookies: ${JSON.stringify(requestExtraInfo.params.blockedCookies, null, 2)}`);

  testRunner.completeTest();
})

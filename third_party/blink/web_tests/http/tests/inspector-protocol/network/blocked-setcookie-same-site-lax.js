(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that making cross origin requests which set SameSite=Lax cookies send us Network.ResponseReceivedExtraInfo events with corresponding blocked set-cookies.\n`);
  await dp.Network.enable();

  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Lax');

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // make a cross origin request to set the cookie, see that it gets blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {responseExtraInfo} = await helper.fetchWithExtraInfo(setCookieUrl);
  testRunner.log(`Javascript initiated subresource blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}\n`);

  // make a cross origin navigation via javascript to set the cookie, see that it is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {responseExtraInfo} = await helper.jsNavigateWithExtraInfo(setCookieUrl);
  testRunner.log(`Javascript initiated navigation blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}\n`);

  // make a cross origin navigation via browser to set the cookie, see that it is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {responseExtraInfo} = await helper.navigateWithExtraInfo(setCookieUrl);
  testRunner.log(`Browser initiated navigation blocked set-cookies: ${JSON.stringify(responseExtraInfo.params.blockedCookies, null, 2)}`);

  testRunner.completeTest();
})

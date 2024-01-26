(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that browser cross origin navigation requests with SameSite=Strict cookies sends us Network.RequestWillBeSentExtraInfo events with corresponding blocked cookies.\n`);
  await dp.Network.enable();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Strict');
  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  const thirdPartyUrl = 'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html';

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // set the SameSite=Strict cookie
  await helper.navigateWithExtraInfo(setCookieUrl);

  // navigate to a different domain and back from the browser and see that the cookie is not blocked
  await helper.navigateWithExtraInfo(thirdPartyUrl);
  var {requestExtraInfo, responseExtraInfo} = await helper.navigateWithExtraInfo(firstPartyUrl);
  testRunner.log(requestExtraInfo.params.associatedCookies, `Browser initiated navigation blocked cookies:`);

  testRunner.completeTest();
})

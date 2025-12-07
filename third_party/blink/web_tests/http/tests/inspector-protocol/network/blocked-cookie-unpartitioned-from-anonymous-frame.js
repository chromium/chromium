(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that unpartitioned cookie access is blocked with correct reason from anonymous frame.\n`);

  await dp.Network.enable();

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie=';
  const iframeURL = 'https://cookie.test:8443/inspector-protocol/network/resources/page-with-credentialless-iframe.html'

  // Set an unpartitioned cookie
  await helper.navigateWithExtraInfo(setCookieUrl + encodeURIComponent(`unpartitioned=value`));

  await helper.navigateWithExtraInfo(iframeURL);

  // Set a cookie from the frame which will set with an opaque partition key
  await helper.jsNavigateIFrameWithExtraInfo('credentialless-iframe', setCookieUrl + encodeURIComponent(`opaque=value`));

  const [request, requestExtraInfo, responseExtraInfo, response] =
    await helper.jsNavigateIFrameWithExtraInfo('credentialless-iframe', iframeURL);

  // See that the unpartitioned cookie is blocked and the opaque one is allowed
  testRunner.log(requestExtraInfo.params.associatedCookies, 'requestWillBeSentExtraInfo associated cookies:');
  testRunner.completeTest();
})

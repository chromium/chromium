(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that webbundle events are triggered`);

  await dp.Network.enable();

  const reportError = event => testRunner.log(event, 'Error: ');
  dp.Network.onSubresourceWebBundleMetadataError(reportError);
  dp.Network.onSubresourceWebBundleInnerResponseError(reportError);
  session.navigate(testRunner.url('./resources/page-with-webbundle.html'));

  const requestWillBeSent = [];
  const [, webBundleMetadataReceived, webBundleInnerResponse] =
      (await Promise.all([
        dp.Network.onceRequestWillBeSent((event) => {
          requestWillBeSent.push(event.params);
          return requestWillBeSent.length == 3;
        }),
        dp.Network.onceSubresourceWebBundleMetadataReceived(),
        dp.Network.onceSubresourceWebBundleInnerResponseParsed()
      ])).map(event => event.params);

  testRunner.log(requestWillBeSent, 'requestWillBeSent', [
    'timestamp', 'wallTime', 'loaderId', 'frameId', 'requestId', 'User-Agent'
  ]);
  testRunner.log(webBundleMetadataReceived, 'webBundleMetadataReceived');
  testRunner.log(
      webBundleInnerResponse, 'webBundleInnerResponse',
      ['bundleRequestId', 'innerRequestId']);

  testRunner.log(
      `webBundleMetadataReceived.urls: ${webBundleMetadataReceived.urls}`);
  testRunner.log(`webBundleInnerResponse.innerRequestURL: ${
      webBundleInnerResponse.innerRequestURL}`);
  if (requestWillBeSent[1].requestId === webBundleMetadataReceived.requestId) {
    testRunner.log(
        'bundle request ID from webBundleMetadataReceived ' +
        'matches ID from requestWillBeSent');
  }
  if (requestWillBeSent[2].requestId ===
      webBundleInnerResponse.innerRequestId) {
    testRunner.log(
        'inner request ID from webBundleInnerResponse ' +
        'matches ID from requestWillBeSent');
  }
  if (webBundleInnerResponse.bundleRequestId ===
      webBundleMetadataReceived.requestId) {
    testRunner.log(
        'inner request ID from webBundleInnerResponse ' +
        'matches ID from webBundleMetadataReceived');
  }

  testRunner.completeTest();
})

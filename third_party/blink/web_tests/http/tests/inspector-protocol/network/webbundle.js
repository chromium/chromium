(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that webbundle events are triggered`);

  await dp.Network.enable();

  let requestWillBeSent = [];
  let webBundleMetadataReceived = [];
  let webBundleInnerResponse = [];

  const recordEvent = (dest, event) => dest.push(event.params);

  dp.Network.onRequestWillBeSent(recordEvent.bind(null, requestWillBeSent));
  dp.Network.onSubresourceWebBundleMetadataReceived(
      recordEvent.bind(null, webBundleMetadataReceived));
  dp.Network.onSubresourceWebBundleInnerResponseParsed(
      recordEvent.bind(null, webBundleInnerResponse));

  const reportError = event => testRunner.log(event, 'Error: ');

  dp.Network.onSubresourceWebBundleMetadataError(reportError);
  dp.Network.onSubresourceWebBundleInnerResponseError(reportError);
  session.navigate(testRunner.url('./resources/page-with-webbundle.html'));
  await dp.Network.onceSubresourceWebBundleInnerResponseParsed();

  testRunner.log(requestWillBeSent, 'requestWillBeSent', [
    'timestamp', 'wallTime', 'loaderId', 'frameId', 'requestId', 'User-Agent'
  ]);
  testRunner.log(webBundleMetadataReceived, 'webBundleMetadataReceived');
  testRunner.log(
      webBundleInnerResponse, 'webBundleInnerResponse',
      ['bundleRequestId', 'innerRequestId']);

  testRunner.log(`webBundleMetadataReceived[0].urls: ${
      webBundleMetadataReceived[0].urls}`);
  testRunner.log(`webBundleInnerResponse[0].innerRequestURL: ${
      webBundleInnerResponse[0].innerRequestURL}`);
  if (requestWillBeSent[1].requestId ===
      webBundleMetadataReceived[0].requestId) {
    testRunner.log(
        'bundle request ID from webBundleMetadataReceived ' +
        'matches ID from requestWillBeSent');
  }
  if (requestWillBeSent[2].requestId ===
      webBundleInnerResponse[0].innerRequestId) {
    testRunner.log(
        'inner request ID from webBundleInnerResponse ' +
        'matches ID from requestWillBeSent');
  }
  if (webBundleInnerResponse[0].bundleRequestId ===
      webBundleMetadataReceived[0].requestId) {
    testRunner.log(
        'inner request ID from webBundleInnerResponse ' +
        'matches ID from webBundleMetadataReceived');
  }

  testRunner.completeTest();
})

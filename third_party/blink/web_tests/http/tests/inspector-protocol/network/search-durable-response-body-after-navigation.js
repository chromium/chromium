(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that searching in response body works after navigation with DurableMessages enabled.`);

  await dp.Network.enable(
      {maxTotalBufferSize: 115025, enableDurableMessages: true});
  await dp.Page.enable();

  const resourceUrl = testRunner.url('./resources/final.js');
  session.evaluate(`fetch("${resourceUrl}").then(r => r.text());`);
  const requestWillBeSent = (await dp.Network.onceRequestWillBeSent()).params;
  testRunner.log(`Request for ${requestWillBeSent.request.url}`);
  await dp.Network.onceLoadingFinished();
  const resourceRequestId = requestWillBeSent.requestId;

  // Search in response body before navigation works locally in renderer.
  const searchBefore = await dp.Network.searchInResponseBody(
      {requestId: resourceRequestId, query: 'hello'});
  testRunner.log('Search before navigation result count: ' +
                 (searchBefore.result?.result?.length ?? searchBefore.error));

  // Perform a navigation to reset renderer state while DurableMessages
  // preserves the response in browser/network process.
  testRunner.log('-- Test Page.navigate() to a cross origin URL --');
  await session.navigate(
      'https://thirdparty.test:8443/inspector-protocol/network/resources/hello-world.html');

  // Verify searchInResponseBody succeeds after navigation via
  // DurableMessageCollector in browser process.
  const searchAfter = await dp.Network.searchInResponseBody(
      {requestId: resourceRequestId, query: 'hello'});
  testRunner.log('Search after navigation result count: ' +
                 (searchAfter.result?.result?.length ?? searchAfter.error));

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests blocking requests with Fetch.failRequest.`);

  await dp.Network.enable();
  await dp.Fetch.enable();
  await dp.Page.enable();

  const urlsById = new Map();
  const logs = [];

  dp.Network.onRequestWillBeSent(event => {
    urlsById.set(event.params.requestId,
                 testRunner.trimURL(event.params.request.url));
  });

  dp.Network.onLoadingFailed(event => {
    const url = urlsById.get(event.params.requestId);
    logs.push(`Network.loadingFailed for ${url}: blockedReason=${
        event.params.blockedReason}`);
  });

  dp.Fetch.onRequestPaused(event => {
    const url = event.params.request.url;
    const trimmedUrl = testRunner.trimURL(url);
    logs.push(`Fetch.requestPaused: ${trimmedUrl}`);

    if (url.endsWith('test.jpg') || url.endsWith('Ahem.ttf') ||
        url.endsWith('empty.html')) {
      dp.Fetch.failRequest(
          {requestId: event.params.requestId, errorReason: 'BlockedByClient'});
    } else {
      dp.Fetch.continueRequest({requestId: event.params.requestId});
    }
  });

  await session.navigate(
      testRunner.url('./resources/resource-cancel-test.html'));
  logs.sort();
  for (const log of logs)
    testRunner.log(log);

  testRunner.log('Navigation complete.');

  testRunner.completeTest();
})

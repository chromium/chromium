(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startURL(
      '../resources/stylesheet-loading-issues.html',
      `Tests that the initiator position is precise on @import style rules`);

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Page.enable();
  await dp.Network.enable();

  dp.Page.reload();

  const sentRequests = [];
  const failedRequests = [];

  const sentRequestsPromise = dp.Network.onceRequestWillBeSent(e => {
    if (e.params.type === 'Stylesheet') {
      sentRequests.push(e.params);
    }
    return sentRequests.length >= 4;
  });

  const failedRequestsPromise = dp.Network.onceLoadingFailed(e => {
    failedRequests.push(e.params);
    return failedRequests.length >= 3;
  });

  await Promise.all([failedRequestsPromise, sentRequestsPromise]);

  for (const stylesheet of ['404.css', '406.css', '408.css']) {
    const {requestId, initiator: {type, url, lineNumber, columnNumber}} =
        sentRequests.find(r => r.request.url.includes(stylesheet));
    const {errorText, canceled} =
        failedRequests.find(r => r.requestId === requestId);

    testRunner.log(`initiator type: ${type}\ninitiator url:${url} ${
        lineNumber +
        1}:${columnNumber + 1}\nerror: ${errorText} (canceled: ${canceled})`);
  }

  testRunner.completeTest();
})

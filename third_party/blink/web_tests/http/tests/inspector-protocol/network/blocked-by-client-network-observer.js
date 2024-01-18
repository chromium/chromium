(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that 'BlockedByClient' network interception is executed with blockedReason set to 'inspector' (=devtools).`);

  await dp.Network.enable();
  await dp.Page.enable();
  await dp.Network.setRequestInterception({
    patterns: [{ urlPattern: '*' }]
  });
  const messagesByURL = new Map();
  function addMessage(url, message) {
    let messages = messagesByURL.get(url);
    if (!messages) {
      messages = [];
      messagesByURL.set(url, messages);
    }
    messages.push(message);
  }
  dp.Network.onRequestIntercepted(event => {
    const url = event.params.request.url;
    if (url.endsWith('page-with-subresource.html')) {
      addMessage(url, '2 - continuing to load');
      dp.Network.continueInterceptedRequest({
        interceptionId: event.params.interceptionId,
      });
    } else {
      addMessage(url, '2 - blocking');
      dp.Network.continueInterceptedRequest({
        interceptionId: event.params.interceptionId,
        errorReason: 'BlockedByClient'
      });
    }
  });

  // We map the requestIds to URLs since these are stable within this test.
  // Also this shows both the requests that we're blocking and the ones that
  // continue.
  const urlByRequestId = new Map();
  dp.Network.onRequestWillBeSent(event => {
    const url = event.params.request.url;
    addMessage(url, '1 - request will be sent');
    urlByRequestId.set(event.params.requestId, url);
  });

  // Log the requests that are blocked, with the blockedReason indicating
  // that they were blocked via the devtools protocol (inspector).
  dp.Network.onLoadingFailed(event => {
    const url = urlByRequestId.get(event.params.requestId);
    const blockedReason = event.params.blockedReason;
    addMessage(url, `3 - loading failed, blockedReason = ${blockedReason}`);
  });

  await dp.Runtime.enable();
  // Test blocking a page. This page doesn't exist so it would 404 but that's
  // OK because the interception / blocking happens before any network fetch
  // would take place anyway.
  await page.navigate('./resources/to-be-blocked.html');
  // This page will not be blocked so it just passes through in the interception.
  // The resource exists, and references to-be-blocked.jpg, a subresource.
  await page.navigate('./resources/page-with-subresource.html');
  for (const url of Array.from(messagesByURL.keys()).sort()) {
    testRunner.log(url);
    for (const message of messagesByURL.get(url).sort())
      testRunner.log(` ${message}`);
  }
  testRunner.completeTest();
})

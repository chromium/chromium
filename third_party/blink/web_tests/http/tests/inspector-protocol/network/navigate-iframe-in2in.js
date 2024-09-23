(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that navigating from a in-process iframe to in-process iframe sets the right sessionId.\n`);

  await dp.Page.enable();
  await dp.Network.clearBrowserCache();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.enable();

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  // Wait for idle instead of load to flush network events caused by this
  // navigation.
  await session.navigate('resources/page-in.html', 'networkIdle');

  const events = [];
  // The two in-process iframes from page-in.html have loaded.
  dp.Network.onRequestWillBeSent(() => events.push("onRequestWillBeSent"));
  dp.Network.onRequestWillBeSentExtraInfo(() => events.push("onRequestWillBeSentExtraInfo"));
  dp.Network.onResponseReceivedExtraInfo(() => events.push("onResponseReceivedExtraInfo"));
  dp.Network.onResponseReceived(() => events.push("onResponseReceived"));

  const [request, requestExtraInfo, responseExtraInfo, response] =
    await helper.jsNavigateIFrameWithExtraInfo('page-iframe', '/devtools/oopif/resources/inner-iframe.html');

  testRunner.log(`Events received: [${events.sort().join(", ")}]`);
  const requests = [request, requestExtraInfo, responseExtraInfo, response];
  const requestIds = requests.map(x => x.params.requestId);
  const sessionIds = requests.map(x => x.sessionId);

  testRunner.log(`Number of request ids: ${requestIds.length}, unique ids ${new Set(requestIds).size}`);
  testRunner.log(`Number of session ids: ${sessionIds.length}, unique ids ${new Set(sessionIds).size}`);

  testRunner.completeTest();
})

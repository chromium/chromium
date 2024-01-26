(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that navigating from a in-process iframe to in-process iframe sets the right sessionId.\n`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });
  let numberOfLoads = 0;
  dp.Page.onLifecycleEvent(onLifecycleEvent);

  await dp.Network.clearBrowserCache();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.enable();

  const helper = (await testRunner.loadScript('resources/extra-info-helper.js'))(dp, session);

  await session.navigate('resources/page-in.html');

  async function onLifecycleEvent(event) {
    if (event.params.name != "load") return;
    numberOfLoads++;
    if (numberOfLoads != 2) return;
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
  }
})

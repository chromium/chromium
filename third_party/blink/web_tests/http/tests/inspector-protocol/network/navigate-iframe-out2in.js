(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that navigating from a OOPIF to in-process iframe sets the right sessionId.\n`);

  async function enableNetwork(network) {
    await network.clearBrowserCache();
    await network.setCacheDisabled({cacheDisabled: true});
    await network.enable();
  }

  const NETWORK_REQUEST_EVENTS = [
    'Network.requestWillBeSent',
    'Network.requestWillBeSentExtraInfo',
    'Network.responseReceived',
    'Network.responseReceivedExtraInfo',
  ];

  const originalDispatch = DevToolsAPI.dispatchMessage;
  const networkListeners = new Set();
  const eventsByRequestId = {};
  DevToolsAPI.dispatchMessage = function(message) {
    const obj = JSON.parse(message);
    if (!NETWORK_REQUEST_EVENTS.includes(obj.method)) {
      originalDispatch(message);
      return;
    }
    const requestId = obj.params.requestId;
    eventsByRequestId[requestId] = eventsByRequestId[requestId] || [];
    for (const existingEvent of eventsByRequestId[requestId]) {
      if (existingEvent.sessionId !== obj.sessionId) {
        testRunner.log(`Session ID mismatch between ${
            JSON.stringify(existingEvent)} and ${message}`);
      }
    }
    eventsByRequestId[requestId].push(obj);
    for (const listener of networkListeners) listener(obj);
  };

  function networkRequestEvents(numRequests) {
    let numPendingRequests = numRequests;
    return new Promise((resolve) => {
      const listener = (event) => {
        if (eventsByRequestId[event.params.requestId].length ==
            NETWORK_REQUEST_EVENTS.length) {
          delete eventsByRequestId[event.params.requestId];
          --numPendingRequests;
          if (numPendingRequests == 0) {
            networkListeners.delete(listener);
            resolve();
          }
        }
      };
      networkListeners.add(listener);
    });
  }

  await dp.Page.enable();
  await enableNetwork(dp.Network);
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});


  session.navigate('resources/page-out.html');
  await Promise.all([
    dp.Target.onceAttachedToTarget().then(async (event) => {
      const dp2 = session.createChild(event.params.sessionId).protocol;
      await dp2.Page.enable();
      await enableNetwork(dp2.Network);
      await dp2.Runtime.runIfWaitingForDebugger();
    }),
    networkRequestEvents(2),  // One request for the main frame and one for the iframe
  ]);

  testRunner.log(
      'Loaded page-out with OOPIF, setting iframe src to in-process URL.');

  session.evaluate(`document.getElementById('page-iframe').src =
      'http://127.0.0.1:8000/inspector-protocol/network/resources/inner-iframe.html'`);
  await networkRequestEvents(1);
  testRunner.log('Got expected iframe events');

  for (const events in Object.values(eventsByRequestId)) {
    for (const event of events) {
      testRunner.log(event, 'Unexpected event');
    }
  }

  testRunner.completeTest();
})

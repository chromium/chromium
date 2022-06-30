(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that navigating from a OOPIF to in-process iframe sets the right sessionId.\n`);

  async function enableNetwork(network) {
    await network.clearBrowserCache();
    await network.setCacheDisabled({cacheDisabled: true});
    await network.enable();
  }

  const NETWORK_REQUEST_EVENTS = [
    'RequestWillBeSent',
    'RequestWillBeSentExtraInfo',
    'ResponseReceived',
    'ResponseReceivedExtraInfo',
  ];

  function getEventName(event) {
    const name = event.method.split('.')[1];
    return name.charAt(0).toUpperCase() + name.slice(1);
  }

  function networkRequestEvents(network, label, expected) {
    return new Promise((resolve) => {
      for (const eventName of NETWORK_REQUEST_EVENTS) {
        network['on' + eventName]((event) => {
          const eventName = getEventName(event);
          let index = expected.indexOf(eventName);
          if (index == -1)  {
            testRunner.log(`Unexpected ${label} event: ${eventName}`);
          } else {
            expected.splice(index, 1);
            if (!expected.length) resolve();
          }
        });
      }
    });
  }

  await dp.Page.enable();
  await enableNetwork(dp.Network);
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const expectedIframeEvents = [];
  let iframeEvents = null;

  session.navigate('resources/page-out.html');
  await Promise.all([
    dp.Target.onceAttachedToTarget().then(async (event) => {
      const dp2 = session.createChild(event.params.sessionId).protocol;
      await dp2.Page.enable();
      await enableNetwork(dp2.Network);
      await dp2.Runtime.runIfWaitingForDebugger();
      iframeEvents =
          networkRequestEvents(dp2.Network, 'iframe', expectedIframeEvents);
    }),
    networkRequestEvents(
        dp.Network, 'main',
        [
          ...NETWORK_REQUEST_EVENTS,  // main frame
          ...NETWORK_REQUEST_EVENTS   // iframe
        ])
  ]);

  testRunner.log(
      'Loaded page-out with OOPIF, setting iframe src to in-process URL.');

  expectedIframeEvents.push(...NETWORK_REQUEST_EVENTS);
  session.evaluate(`document.getElementById('page-iframe').src =
      'http://127.0.0.1:8000/inspector-protocol/network/resources/inner-iframe.html'`);
  await iframeEvents;
  testRunner.log('Got expected iframe events');

  testRunner.completeTest();
})

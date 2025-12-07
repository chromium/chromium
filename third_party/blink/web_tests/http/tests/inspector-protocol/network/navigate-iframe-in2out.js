(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that navigating from a OOPIF to in-process iframe sets the right sessionId.\n`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });
  let numberOfLoads = 0;
  dp.Page.onLifecycleEvent(onLifecycleEvent);

  await dp.Network.clearBrowserCache();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.enable();

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const iFrameEvents = new Map();
  const mainEvents = new Map();

  function hook(network, events) {
    network.onRequestWillBeSent(() => events.set('onRequestWillBeSent', (events.get('onRequestWillBeSent') ?? 0) + 1));
    network.onRequestWillBeSentExtraInfo(() => events.set('onRequestWillBeSentExtraInfo', (events.get('onRequestWillBeSentExtraInfo') ?? 0) + 1));
    network.onResponseReceivedExtraInfo(() => events.set('onResponseReceivedExtraInfo', (events.get('onResponseReceivedExtraInfo') ?? 0) + 1));
    network.onResponseReceived(() => events.set('onResponseReceived', (events.get('onResponseReceived') ?? 0) + 1));
  }

  dp.Target.onAttachedToTarget(async event => {
   const dp2 = session.createChild(event.params.sessionId).protocol;
   await dp2.Page.enable();
   await dp2.Page.setLifecycleEventsEnabled({ enabled: true });
   dp2.Page.onLifecycleEvent(onLifecycleEvent);
   await dp2.Network.clearBrowserCache();
   await dp2.Network.setCacheDisabled({cacheDisabled: true});
   await dp2.Network.enable();
   // None of these should fire.
   hook(dp2.Network, iFrameEvents);
   await dp2.Runtime.runIfWaitingForDebugger();
  });

  await session.navigate('resources/page-in.html');

  async function onLifecycleEvent(event) {
    if (event.params.name != 'load') return;
    numberOfLoads++;
    if (numberOfLoads == 4) {
      // There are two load events fired, one for the OOPIF frame, and one for page-in after
      // setting the src property on the iframe.
      for (eventName of ['onRequestWillBeSent', 'onRequestWillBeSentExtraInfo', 'onResponseReceived', 'onResponseReceivedExtraInfo']) {
        const eventCount = iFrameEvents.get(eventName);
        if (eventCount) {
          testRunner.log(`Received ${eventCount} unexpected '${eventName}' events on iframe.`);
        }
      }
      if (mainEvents.get('onRequestWillBeSent') !== 1) {
        testRunner.log('Unexpected number of \'onRequestWillBeSent\' events received on main frame:');
        testRunner.log(`Actual: ${mainEvents.get('onRequestWillBeSent')}, Expected: 1`);
      }
      if (mainEvents.get('onRequestWillBeSentExtraInfo') < 1) {
        testRunner.log('Unexpected number of \'onRequestWillBeSentExtraInfo\' events received on main frame:');
        testRunner.log(`Actual: ${mainEvents.get('onRequestWillBeSentExtraInfo')}, Expected: >=1`);
      }
      if (mainEvents.get('onResponseReceived') !== 1) {
        testRunner.log('Unexpected number of \'onResponseReceived\' events received on main frame:');
        testRunner.log(`Actual: ${mainEvents.get('onResponseReceived')}, Expected: 1`);
      }
      if (mainEvents.get('onResponseReceivedExtraInfo') < 1) {
        testRunner.log('Unexpected number of \'onResponseReceivedExtraInfo\' events received on main frame');
        testRunner.log(`Actual: ${mainEvents.get('onResponseReceivedExtraInfo')}, Expected: >=1`);
      }
      testRunner.completeTest();
    }
    if (numberOfLoads == 2) {
      testRunner.log('Loaded page-in with in-process iframe, setting iframe src to out-of-process URL.');

      hook(dp.Network, mainEvents);

      const iFrameId = 'page-iframe';
      const url = 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/inner-iframe.html';
      await session.evaluate(`document.getElementById('${iFrameId}').src = '${url}'`);
    }
  }
})

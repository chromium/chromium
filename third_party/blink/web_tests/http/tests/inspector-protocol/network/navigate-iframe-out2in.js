(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that ExtraInfo CDP events are generated when navigating from a OOPIF to in-process iframe.\n`);

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
  for (eventName of ['onRequestWillBeSent', 'onRequestWillBeSentExtraInfo', 'onResponseReceived', 'onResponseReceivedExtraInfo']) {
    mainEvents.set(eventName, 0);
    iFrameEvents.set(eventName, 0);
  }

  function verifyEventCount(eventName, totalCount, mainCount) {
    if (totalCount !== mainEvents.get(eventName) + iFrameEvents.get(eventName)) {
      testRunner.log(`Unexpected number of '${eventName}' events received:`);
      testRunner.log(`Actual: ${mainEvents.get(eventName) + iFrameEvents.get(eventName)}, Expected: ${totalCount}`);
    }
    if (mainCount !== undefined) {
      if (mainCount !== mainEvents.get(eventName)) {
        testRunner.log(`Unexpected number of '${eventName}' events received on main frame:`);
        testRunner.log(`Actual: ${mainEvents.get(eventName)}, Expected: ${mainCount}`);
      }
      if (totalCount - mainCount !== iFrameEvents.get(eventName)) {
        testRunner.log(`Unexpected number of '${eventName}' events received on iframe:`);
        testRunner.log(`Actual: ${iFrameEvents.get(eventName)}, Expected: ${totalCount - mainCount}`);
      }
    }
  }

  function hook(network, events) {
    network.onRequestWillBeSent(() => events.set('onRequestWillBeSent', events.get('onRequestWillBeSent') + 1));
    network.onResponseReceived(() => events.set('onResponseReceived', events.get('onResponseReceived') + 1));
    network.onRequestWillBeSentExtraInfo(() => events.set('onRequestWillBeSentExtraInfo', events.get('onRequestWillBeSentExtraInfo') + 1));
    network.onResponseReceivedExtraInfo(() => events.set('onResponseReceivedExtraInfo', events.get('onResponseReceivedExtraInfo') + 1));
  }

  dp.Target.onAttachedToTarget(async event => {
   const dp2 = session.createChild(event.params.sessionId).protocol;
   await dp2.Page.enable();
   await dp2.Page.setLifecycleEventsEnabled({ enabled: true });
   dp2.Page.onLifecycleEvent(onLifecycleEvent);
   await dp2.Network.clearBrowserCache();
   await dp2.Network.setCacheDisabled({cacheDisabled: true});
   await dp2.Network.enable();
   hook(dp2.Network, iFrameEvents);
   await dp2.Runtime.runIfWaitingForDebugger();
  });

  hook(dp.Network, mainEvents);
  await session.navigate('resources/page-out.html');

  async function onLifecycleEvent(event) {
    if (event.params.name !== 'load') return;
    numberOfLoads++;
    if (numberOfLoads === 4) {
      verifyEventCount('onRequestWillBeSent', 3, 2);
      verifyEventCount('onResponseReceived', 3, 2);
      // `ExtraInfo`-events are usually generated before the navigation is committed, and the `ExtraInfo`-events are sent for the OOPIF-target.
      // But there is no guarantee for this ordering, and sometimes the navigation is committed first, which causes the destruction of the
      // OOPIF's `DevToolsRenderAgentHost`. In this case, the `ExtraInfo`-events are sent via the main frame's `DevToolsAgentHost` instead.
      verifyEventCount('onRequestWillBeSentExtraInfo', 3);
      verifyEventCount('onResponseReceivedExtraInfo', 3);

      testRunner.log('In-process iframe loading complete.');
      testRunner.completeTest();
    }
    if (numberOfLoads === 2) {
      verifyEventCount('onRequestWillBeSent', 2, 2);
      verifyEventCount('onResponseReceived', 2, 2);
      verifyEventCount('onRequestWillBeSentExtraInfo', 2, 2);
      verifyEventCount('onResponseReceivedExtraInfo', 2, 2);

      testRunner.log('Loaded page-out with OOPIF, setting iframe src to in-process URL.');
      await session.evaluate(`document.getElementById('page-iframe').src =
        'http://127.0.0.1:8000/inspector-protocol/network/resources/inner-iframe.html'`);
    }
  }
})

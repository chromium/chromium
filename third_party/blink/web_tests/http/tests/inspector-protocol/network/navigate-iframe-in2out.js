(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that navigating from a OOPIF to in-process iframe sets the right sessionId.\n`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });
  let numberOfLoads = 0;
  dp.Page.onLifecycleEvent(onLifecycleEvent);

  await dp.Network.clearBrowserCache();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.enable();

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const iFrameEvents = [];
  const mainEvents = [];

  function hook(network, events) {
    network.onRequestWillBeSent(() => events.push("onRequestWillBeSent"));
    network.onRequestWillBeSentExtraInfo(() => events.push("onRequestWillBeSentExtraInfo"));
    network.onResponseReceivedExtraInfo(() => events.push("onResponseReceivedExtraInfo"));
    network.onResponseReceived(() => events.push("onResponseReceived"));
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
    if (event.params.name != "load") return;
    numberOfLoads++;
    if (numberOfLoads == 4) {
      // There are two load events fired, one for the OOPIF frame, and one for page-out after
      // setting the src property on the iframe.
      testRunner.log(`Events received on iframe: [${iFrameEvents.sort().join(", ")}]`);
      testRunner.log(`Events received on main frame: [${mainEvents.sort().join(", ")}]`);
      testRunner.completeTest();
    }
    if (numberOfLoads == 2) {
      testRunner.log("Loaded page-out with OOPIF, setting iframe src to in-process URL.");

      hook(dp.Network, mainEvents);

      const iFrameId = 'page-iframe';
      const url = 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/inner-iframe.html';
      await session.evaluate(`document.getElementById('${iFrameId}').src = '${url}'`);
    }
  }
})

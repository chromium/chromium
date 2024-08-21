(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    'Verifies that we can intercept the network request for the script loaded when adding a module to an audio worklet.');

    await Promise.all([
      dp.Target.setDiscoverTargets({discover: true}),
      dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true}),
    ]);
    const swTargetPromises = [
      dp.Target.onceTargetCreated(),
      new Promise(resolve => {
        dp.Target.onceAttachedToTarget(async event => {
          const swdp = session.createChild(event.params.sessionId).protocol;
          const networkEnableRes = await swdp.Network.enable();

          swdp.Network.onRequestWillBeSent(e => {
            if (e.params.request.url.endsWith('audio-worklet-processor.js')) {
              resolve([networkEnableRes, `Network.requestWillBeSent: ${e.params.request.url}`]);
            }
          });

          swdp.Runtime.runIfWaitingForDebugger();
        })
      }),
    ];

    await session.evaluate(
      `new AudioContext().audioWorklet.addModule('/inspector-protocol/network/resources/audio-worklet-processor.js')`);
    const [_swTarget, [networkEnableRes, scriptFetched]] = await Promise.all(swTargetPromises);
    testRunner.log(networkEnableRes);
    testRunner.log(scriptFetched);
    testRunner.log("OK");
    testRunner.completeTest();
});

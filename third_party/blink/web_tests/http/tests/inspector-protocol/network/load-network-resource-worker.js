(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
    'Verifies that we can retrieve a request body consisting of blob in service worker.');

    await Promise.all([
      dp.Target.setDiscoverTargets({discover: true}),
      dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true}),
    ]);
    const swTargetPromises = [
      dp.Target.onceTargetCreated(),
      dp.Target.onceAttachedToTarget(),
    ];
    await session.evaluate(`new Worker('/inspector-protocol/network/resources/worker.js')`);
    const [swTarget, swAttachedEvent] = await Promise.all(swTargetPromises);
    testRunner.log("OK");

    const swdp = session.createChild(swAttachedEvent.params.sessionId).protocol;
    const result = await swdp.Network.enable();
    testRunner.log(result);
    const url = `http://localhost:8000/inspector-protocol/network/resources/source.map`;
    const response1 = await swdp.Network.loadNetworkResource(
        {url, options: {disableCache: false, includeCredentials: false}});

    testRunner.log(response1, `Response for fetch with existing resource: `, ["headers", "id", "sessionId"]);
    testRunner.completeTest();
});

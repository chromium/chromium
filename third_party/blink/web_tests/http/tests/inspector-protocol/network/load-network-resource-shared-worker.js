(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
    'resources/page-with-shared-worker.html',
    `Tests that we can load resources from a dedicated worker.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});

  await Promise.all([
    target.setDiscoverTargets({discover: true}),
    target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true}),
  ]);

  const response = await target.getTargets();
  const sharedWorkers = response.result.targetInfos.filter(info => info.type === "shared_worker");
  testRunner.log(sharedWorkers.length, `Number of discovered shared workers`);
  const [sharedWorker] = sharedWorkers;
  const {result} = await target.attachToTarget({targetId: sharedWorker.targetId, flatten: true});
  const swdp = session.createChild(result.sessionId).protocol;
  await swdp.Network.enable();
  const url = `http://localhost:8000/inspector-protocol/network/resources/source.map`;
  const response1 = await swdp.Network.loadNetworkResource(
      {url, options: {disableCache: false, includeCredentials: false}});
  testRunner.log(response1.result, `Response for fetch with existing resource: `, ["headers", "stream"]);
  testRunner.completeTest();
});

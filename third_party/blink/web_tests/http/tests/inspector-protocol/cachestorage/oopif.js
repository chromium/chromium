(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://127.0.0.1:8443',
      `Tests that OOPIF CacheStorage can also be can be accessed.`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  session.evaluate(`
    const frame = document.createElement('iframe');
    frame.src = 'https://devtools.oopif.test:8443/';
    document.body.appendChild(frame);`);

  const oopifAttached = await dp.Target.onceAttachedToTarget();
  const oopifSession = session.createChild(oopifAttached.params.sessionId);
  const oopifProtocol = oopifSession.protocol;

  const swHelper = (await testRunner.loadScript('../service-worker/resources/service-worker-helper.js'))(oopifProtocol, oopifSession);
  await oopifProtocol.Runtime.enable();
  await oopifProtocol.ServiceWorker.enable();
  await oopifProtocol.Runtime.runIfWaitingForDebugger();
  const swActivated =
      swHelper.installSWAndWaitForActivated('/inspector-protocol/cachestorage/resources/service-worker.js');

  const swAttached = await dp.Target.onceAttachedToTarget();
  const swProtocol = session.createChild(swAttached.params.sessionId).protocol;
  await swProtocol.Runtime.enable();
  await swProtocol.Runtime.runIfWaitingForDebugger();
  await swActivated;

  const oopifId =
      (await oopifProtocol.Page.getResourceTree()).result.frameTree.frame.id;
  const oopifStorageKey = (await oopifProtocol.Storage.getStorageKeyForFrame({
                            frameId: oopifId
                          })).result.storageKey;

  const {result} = await oopifProtocol.CacheStorage.requestCacheNames({storageKey: oopifStorageKey});
  testRunner.log(result);

  testRunner.completeTest()
});

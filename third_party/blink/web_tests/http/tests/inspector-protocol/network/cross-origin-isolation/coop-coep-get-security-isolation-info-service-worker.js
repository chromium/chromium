(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
    'Verifies that we can successfully retrieve the security isolation status of a service worker.');
  const swHelper = (await testRunner.loadScript('../../service-worker/resources/service-worker-helper.js'))(dp, session);

  let swdp = null;
  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    swdp = session.createChild(event.params.sessionId).protocol;
  });

  const serviceWorkerURL = '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);

  swdp.Network.enable();
  const {result} = await swdp.Network.getSecurityIsolationStatus({frameId: ''});
  testRunner.log(`COEP status`);
  testRunner.log(result.status.coep);
  testRunner.log(`COOP status`);
  testRunner.log(result.status.coop);

  testRunner.completeTest();
});

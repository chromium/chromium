(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const worker = `console.log("Worker");
  onmessage = function(e) {
    console.log(e);
  };`;
  const baseUrl =
      `https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php`;
  const workerUrl = `${baseUrl}?coep&corp=same-site&coop&content=${
      encodeURIComponent(worker)}&ctype=application/javascript`;
  const url = `${baseUrl}?coep&corp=same-site&coop`;
  var {page, session, dp} = await testRunner.startURL(
      url,
      'Verifies that we can successfully retrieve the security isolation status of a dedicated worker.');

  await Promise.all([
    dp.Target.setDiscoverTargets({discover: true}),
    dp.Target.setAutoAttach(
        {autoAttach: true, waitForDebuggerOnStart: false, flatten: true}),
  ]);
  const swTargetPromises = [
    dp.Target.onceTargetCreated(),
    dp.Target.onceAttachedToTarget(),
  ];

  setTimeout(() => testRunner.completeTest(), 5000);

  await session.navigate(`${url}`);

  const workerStarted = await session.evaluate(`new Worker('${workerUrl}')`);
  testRunner.log(`Worker started successfully: ${workerStarted !== undefined}`);

  const [swTarget, swAttachedEvent] = await Promise.all(swTargetPromises);
  testRunner.log('Connected to worker target');

  const swdp = session.createChild(swAttachedEvent.params.sessionId).protocol;
  await swdp.Network.enable();

  const {result} = await swdp.Network.getSecurityIsolationStatus({frameId: ''});
  testRunner.log(`COEP status`);
  testRunner.log(result.status.coep);
  testRunner.log(`COOP status`);
  testRunner.log(result.status.coop);

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that isolation status is reported correctly');

  await dp.Page.enable();
  await dp.Network.enable();

  let event = null;
  do {
    const frameNavigated = Promise.race([
      dp.Page.onceFrameNavigated(),
      new Promise(async (resolve) => {
        dp.Target.onAttachedToTarget(async e => {
          const dp2 = session.createChild(e.params.sessionId).protocol;
          await dp2.Page.enable();
          dp2.Page.onceFrameNavigated().then(resolve);
          dp2.Runtime.runIfWaitingForDebugger();
        });
        await dp.Target.setAutoAttach(
            {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
      }),
    ]);

    session.navigate(
        'https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?coep=require-corp;report-to="endpoint-1"&corp=same-origin&coop=same-origin-allow-popups;report-to="endpoint-2"');
    [event,
    ] = await Promise.all([frameNavigated, dp.Network.oncePolicyUpdated()]);
    // Retry navigation in case the URL couldn't load
  } while (event.params.frame.unreachableUrl);
  const frameId = event.params.frame.id;
  const {result} = await session.protocol.Network.getSecurityIsolationStatus({frameId});

  testRunner.log(`COEP status`);
  testRunner.log(result.status.coep);
  testRunner.log(`COOP status`);
  testRunner.log(result.status.coop);

  testRunner.completeTest();
})


(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that isolation status is reported correctly');

  await dp.Page.enable();

  const results = new Map();

  let recordFameNavigated;
  const frameNavigatedPromise = new Promise(resolve => {
    let numberOfFrameNavigated = 0;
    recordFameNavigated = () => {
      if (++numberOfFrameNavigated === 4) {
        resolve();
      }
    }
  });

  async function onFrameNavigated(event) {
    const frameId = event.params.frame.id;
    const {result} = await session.protocol.Network.getSecurityIsolationStatus({frameId});
    results.set(event.params.frame.url, {coep: result.status.coep, coop: result.status.coop})
    recordFameNavigated();
  }
  dp.Page.onFrameNavigated(onFrameNavigated);

  dp.Target.onAttachedToTarget(async e => {
    const dp2 = session.createChild(e.params.sessionId).protocol;
    await dp2.Page.enable();
    dp2.Page.onFrameNavigated(onFrameNavigated);
    await dp2.Runtime.runIfWaitingForDebugger();
  });
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const url = 'https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php';
  await session.navigate(`${url}?coep&corp=same-site&coop`);
  await session.navigate(`${url}?coep=require-corp;report-to="endpoint-1"&corp=same-origin&coop=same-origin-allow-popups;report-to="endpoint-2"`);
  await session.navigate(`${url}?coep-rpt&corp=same-site&coop-rpt`);
  await session.navigate(`${url}?coep-rpt=require-corp;report-to="endpoint-1"&corp=same-origin&coop-rpt=same-origin-allow-popups;report-to="endpoint-2"`);

  await frameNavigatedPromise;

  for (const key of Array.from(results.keys()).sort()) {
    testRunner.log(key);
    testRunner.log(`COEP status`);
    const {coep, coop} = results.get(key);
    testRunner.log(coep);
    testRunner.log(`COOP status`);
    testRunner.log(coop);
  }

  testRunner.completeTest();
})


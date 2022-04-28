(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Network events are fired for 404'd service workers and they align (classic)`);
  testRunner.startDumpingProtocolMessages();
  const FetchHelper = await testRunner.loadScript('resources/fetch-test.js');
  const NetworkLifecycleObserver =
      await testRunner.loadScript('resources/service-workers/helper.js');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let workerNetworkObserver = new Promise(resolve => {
    dp.Target.onAttachedToTarget(async event => {
      const wdp = session.createChild(event.params.sessionId).protocol;
      resolve(new NetworkLifecycleObserver(wdp));
      wdp.Runtime.runIfWaitingForDebugger();
    });
  });

  const pageObserver = new NetworkLifecycleObserver(dp);
  const observers = [
    pageObserver.waitForCompletion(/main\.html/),
    workerNetworkObserver.then(o => Promise.all([
      o.waitForCompletion(/sw-classic\.js/),
    ]))
  ];

  const globalFetcher = new FetchHelper(testRunner, testRunner.browserP());
  globalFetcher.setLogPrefix('[browser] ');
  await globalFetcher.enable();
  globalFetcher.onRequest().continueRequest({});

  await session.navigate(
      './resources/service-workers/main.html?type=classic&fail=404');
  testRunner.log(`\nNetwork Events and Alignment:`)
  testRunner.log((await Promise.all(observers)).flat().join(`\n`));
  testRunner.completeTest();
})

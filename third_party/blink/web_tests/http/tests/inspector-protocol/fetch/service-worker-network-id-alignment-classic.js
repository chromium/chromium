(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Network events are fired for service workers and they align (classic)`);

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
      o.waitForCompletion(/imported-classic\.js/),
      o.waitForCompletion(/404-me/, 404),
    ]))
  ];

  const globalFetcher = new FetchHelper(testRunner, testRunner.browserP());
  globalFetcher.setLogPrefix('[browser] ');
  await globalFetcher.enable();
  globalFetcher.onRequest().continueRequest({});

  await session.navigate('./resources/service-workers/main.html?type=classic');
  const messages = await session.evaluateAsync('window.completionMessage');
  testRunner.log(
      `\nResults from Running the Page:\n\t${messages.join(`\n\t-> `)}`);
  testRunner.log(`\nNetwork Events and Alignment:`)
  testRunner.log((await Promise.all(observers)).flat().join(`\n`));
  testRunner.completeTest();
})

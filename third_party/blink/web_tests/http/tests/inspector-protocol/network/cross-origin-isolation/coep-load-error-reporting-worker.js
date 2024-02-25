(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that cross-origin embedder policy (COEP) related blocking for worker sources is reported correctly.`);

  await session.protocol.Network.clearBrowserCache();
  await session.protocol.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  setTimeout(() => dump(), 5000);

  let numberOfMessages = 0;
  const expectedNumberOfMessages = 21;
  const resources = new Map();

  function dump() {
    function compareInfo(a, b) {
      return `${a.requestWillBeSent?.request?.url}`.localeCompare(`${b.requestWillBeSent?.request?.url}`);
    }
    const entries = Array.from(resources.values()).sort(compareInfo);
    for (const entry of entries) {
      if (entry.loadingFailed) {
        testRunner.log(`${entry.requestWillBeSent.request?.url}: ${entry.loadingFailed.errorText} ${entry.loadingFailed.blockedReason}`);
      }
      if (entry.loadingFinished) {
        testRunner.log(`${entry.requestWillBeSent.request?.url}: *loading finished*`);
      }
    }
    testRunner.completeTest();
  }

  function record(requestId, info) {
    resources.set(requestId, {...resources.get(requestId), ...info});

    if (++numberOfMessages === expectedNumberOfMessages) {
      dump();
    }
  }

  async function initializeTarget(dp) {
    dp.Network.onLoadingFailed(event => record(event.params.requestId, {loadingFailed: event.params})),
    dp.Network.onLoadingFinished(event => record(event.params.requestId, {loadingFinished: event.params})),
    dp.Network.onRequestWillBeSent(event => record(event.params.requestId, {requestWillBeSent: event.params})),
    await Promise.all([
      dp.Network.enable(),
      dp.Page.enable()
    ]);
  }

  await initializeTarget(dp);

  dp.Target.onAttachedToTarget(async e => {
    const dp = session.createChild(e.params.sessionId).protocol;
    await initializeTarget(dp);
  });

  page.navigate('https://devtools.test:8443/inspector-protocol/network/cross-origin-isolation/resources/coep-page-with-worker.php');

  // `record` above makes sure to complete the test.
})


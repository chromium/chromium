(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    'Verifies that disabling cache in DevTools works when fetching scripts for audio worklets.');

  await Promise.all([
    dp.Target.setDiscoverTargets({discover: true}),
    dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true}),
  ]);

  const initDebuggerAndInterceptResponse = async (cacheDisabled) => {
    const attachEvent = await dp.Target.onceAttachedToTarget();

    const swdp = session.createChild(attachEvent.params.sessionId).protocol;
    await swdp.Network.enable();
    await swdp.Network.setCacheDisabled({cacheDisabled});
    await swdp.Runtime.runIfWaitingForDebugger();

    const [responseReceived] = await Promise.all([
      swdp.Network.onceResponseReceived(),
      swdp.Network.onceLoadingFinished()
    ]);

    const requestId = responseReceived.params.requestId;
    const responseBody = await swdp.Network.getResponseBody({requestId});
    const content = responseBody.result.body;
    if (typeof content != 'string') {
      testRunner.fail(`Invalid response: ${content}`);
    }

    return content;
  }

  async function fetchWorkletScript(cacheDisabled) {
    const resPromise = initDebuggerAndInterceptResponse(cacheDisabled);
    // `cached.php` returns a random number each time it's fetched and sets cache
    // headers on responses. If the response is cached, the random number will be
    // the same as the first fetch.
    await session.evaluate(
      `new AudioContext().audioWorklet.addModule('/inspector-protocol/network/resources/cached.php')`);
    return resPromise;
  }

  testRunner.log('First fetch to populate cache');
  const fetchedContent0 = await fetchWorkletScript(false);

  testRunner.log('Second request (should be cached)');
  const fetchedContent1 = await fetchWorkletScript(false);

  testRunner.log('Third request (cache disabled; should not be cached)');
  const fetchedContent2 = await fetchWorkletScript(true);

  testRunner.log(`Second request cached: ${fetchedContent0 === fetchedContent1}`);
  testRunner.log(`Third request cached: ${fetchedContent0 === fetchedContent2}`);

  testRunner.log('OK');
  testRunner.completeTest();
});

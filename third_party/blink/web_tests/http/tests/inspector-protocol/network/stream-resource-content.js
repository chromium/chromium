(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    './resources/event-stream.html',
    `Test that fetch and XHR responses can be streamed`);

  await dp.Runtime.enable();
  await dp.Network.enable();

  const target = testRunner.browserP().Target;
  await session.evaluateAsync('navigator.serviceWorker.ready');
  const response = await target.getTargets();
  const serviceWorkers = response.result.targetInfos.filter(info => info.type === "service_worker");
  testRunner.log(`Number of discovered service workers: ${serviceWorkers.length}`);
  const [serviceWorker] = serviceWorkers;
  const swSession = await session.attachChild(serviceWorker.targetId);

  async function runTest(script) {
    session.evaluate(script);
    const request = (await dp.Network.onceRequestWillBeSent()).params;
    swSession.evaluate('enqueue("data: test1")');
    testRunner.log(await dp.Network.onceDataReceived(), 'Data received #1: ');
    testRunner.log(await dp.Network.streamResourceContent({
      requestId: request.requestId,
    }), 'Network.streamResourceContent response: ');
    swSession.evaluate('enqueue("data: test2")');
    testRunner.log(await dp.Network.onceDataReceived(), 'Data received #2: ');
    swSession.evaluate('enqueue("data: test3")');
    testRunner.log(await dp.Network.onceDataReceived(), 'Data received #3: ');
    swSession.evaluate('close()');
    await dp.Network.onceLoadingFinished();
  }

  testRunner.log('\nTest fetch:\n');
  await runTest('runFetch();');
  testRunner.log('\nTest XHR:\n');
  await runTest('runXHR();');
  testRunner.completeTest();
});

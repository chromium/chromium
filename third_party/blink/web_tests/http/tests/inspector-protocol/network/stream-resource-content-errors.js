(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    './resources/event-stream.html',
    `Test that fetch responses can be streamed`);
  await dp.Runtime.enable();
  await dp.Network.enable();

  const target = testRunner.browserP().Target;
  await session.evaluateAsync('navigator.serviceWorker.ready');
  const response = await target.getTargets();
  const serviceWorkers = response.result.targetInfos.filter(info => info.type === "service_worker");
  testRunner.log(serviceWorkers.length, `Number of discovered service workers`);
  const [serviceWorker] = serviceWorkers;
  const swSession = await session.attachChild(serviceWorker.targetId);

  session.evaluate('runFetch()');
  const request = (await dp.Network.onceRequestWillBeSent()).params;
  await swSession.evaluate('enqueue("data: test")');
  swSession.evaluate('close()');
  await dp.Network.onceLoadingFinished();
  testRunner.log(await dp.Network.streamResourceContent({
    requestId: request.requestId,
  }), 'Network.streamResourceContent response after request has finished: ');
  testRunner.log(await dp.Network.streamResourceContent({
    requestId: 'wrong',
  }), 'Network.streamResourceContent response on wrong requestId: ');

  testRunner.completeTest();
});

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that requestIntercepted has a requestId corresponding to requestWillBeSent's requestId`);

  await dp.Network.enable();
  await dp.Network.setRequestInterception({patterns: [{urlPattern: '*'}]});

  const [requestWillBeSent, requestIntercepted, evaluate] = await Promise.all([
    dp.Network.onceRequestWillBeSent(),
    dp.Network.onceRequestIntercepted(),
    session.evaluate(`fetch('${testRunner.url('./resources/test.css')}')`)
  ]);

  const idsAreEqual = requestWillBeSent.params.requestId === requestIntercepted.params.requestId;
  testRunner.log(`requestIntercepted.requestId === requestWillBeSent.requestId: ${idsAreEqual}`);
  testRunner.completeTest();
})

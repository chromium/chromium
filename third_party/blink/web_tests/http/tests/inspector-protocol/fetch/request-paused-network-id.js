(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that requestPaused has a networkId corresponding to requestWillBeSent's requestId`);

  await dp.Network.enable();
  await dp.Fetch.enable();

  const [requestWillBeSent, requestPaused, evaluate] = await Promise.all([
    dp.Network.onceRequestWillBeSent(),
    dp.Fetch.onceRequestPaused(),
    session.evaluate(`fetch('${testRunner.url('./resources/hello-world.txt')}')`)
  ]);

  const idsAreEqual = requestWillBeSent.params.requestId === requestPaused.params.networkId;
  testRunner.log(`requestPaused.networkId === requestWillBeSent.requestId: ${idsAreEqual}`);
  testRunner.completeTest();
})

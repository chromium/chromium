(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that requestPaused has a networkId corresponding to requestWillBeSent's requestId on navigation`);

  await dp.Network.enable();
  await dp.Fetch.enable();

  session.navigate(testRunner.url('./resources/hello-world.html'))
  const [requestWillBeSent, requestPaused, navigate] = await Promise.all([
    dp.Network.onceRequestWillBeSent(),
    dp.Fetch.onceRequestPaused(),
  ]);

  const idsAreEqual = requestWillBeSent.params.requestId === requestPaused.params.networkId;
  testRunner.log(`requestPaused.networkId === requestWillBeSent.requestId: ${idsAreEqual}`);
  testRunner.completeTest();
})

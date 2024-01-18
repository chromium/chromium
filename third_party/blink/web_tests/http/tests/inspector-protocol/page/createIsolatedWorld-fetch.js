(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      "Verifies fetch() doesn't cause a crash in isolated world");

  await dp.Runtime.enable();
  await dp.Page.enable();

  const getResourceTreeResponse = await dp.Page.getResourceTree();
  const mainFrameId = getResourceTreeResponse.result.frameTree.frame.id;

  const isolatedWorldResponse = await dp.Page.createIsolatedWorld(
      {frameId: mainFrameId, worldName: 'Test world', grantUniveralAccess: false});
  const contextId = isolatedWorldResponse.result.executionContextId;
  testRunner.log('Got execution context id');

  await dp.Network.enable();

  await dp.Runtime.evaluate({
    contextId: contextId,
    expression: `
        fetch('/inspector-protocol/resources/test-page.html')
    `});

  const responseReceived = await dp.Network.onceResponseReceived();
  testRunner.log('Response received');
  testRunner.log('  url: ' + responseReceived.params.response.url);
  testRunner.log('  type: ' + responseReceived.params.type);

  testRunner.completeTest();
})

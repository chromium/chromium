(async function(testRunner) { const {page, session, dp} = await testRunner.startBlank(
      "Verifies that isolated worlds can be created after same-process navigation");

  await dp.Runtime.enable();
  await dp.Page.enable();

  const getResourceTreeResponse = await dp.Page.getResourceTree();
  const mainFrameId = getResourceTreeResponse.result.frameTree.frame.id;

  await evaluateInIsolatedWorld(mainFrameId);
  testRunner.log('Reload.');
  await Promise.all([
    dp.Page.reload(),
    dp.Page.onceLoadEventFired(),
  ]);
  await evaluateInIsolatedWorld(mainFrameId);

  testRunner.completeTest();

  async function evaluateInIsolatedWorld(frameId) {
    const isolatedWorldResponse = await dp.Page.createIsolatedWorld(
        {frameId, worldName: 'Test world', grantUniveralAccess: false});
    const contextId = isolatedWorldResponse.result.executionContextId;
    const response = await dp.Runtime.evaluate({
      contextId: contextId,
      expression: `7 * 8`,
    });
    testRunner.log(`Isolated world evaluation result: ${response.result.result.value}`);
  }
})

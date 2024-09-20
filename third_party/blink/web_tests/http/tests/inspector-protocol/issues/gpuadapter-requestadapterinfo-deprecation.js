(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that GPUAdapter requestAdapterInfo() deprecation issues are reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate(`(async function() {
    const adapter = await navigator.gpu.requestAdapter();
    await adapter.requestAdapterInfo();
  })()`);
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

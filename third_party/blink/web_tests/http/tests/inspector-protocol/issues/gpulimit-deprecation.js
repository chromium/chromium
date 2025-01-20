(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that WebGPU limit maxInterStageShaderComponents deprecation issues are reported`);
  await dp.Audits.enable();
  const issue1 = dp.Audits.onceIssueAdded();
  const issue2 = dp.Audits.onceIssueAdded();
  session.evaluate(`(async function() {
    const adapter = await navigator.gpu.requestAdapter();
    const maxInterStageShaderComponents = adapter.limits.maxInterStageShaderComponents;
    await adapter.requestDevice({requiredLimits: { maxInterStageShaderComponents }});
  })()`);
  const result1 = await issue1;
  const result2 = await issue2;
  testRunner.log(result1.params, "Inspector issue: ");
  testRunner.log(result2.params, "Inspector issue: ");
  testRunner.completeTest();
})

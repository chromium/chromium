(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { session, dp } = await testRunner.startBlank(`Tests that GPUAdapter isFallbackAdapter deprecation issues are reported`);
    await dp.Audits.enable();
    const issue = dp.Audits.onceIssueAdded();
    const adapter = await session.evaluateAsync(async () => {
      const a = await navigator.gpu.requestAdapter();
      console.log(a?.isFallbackAdapter);
      return `${a}`;
    });
    if (adapter === "null") {
      testRunner.log("No adapter");
      testRunner.completeTest();
      return;
    }
    const result = await issue;
    testRunner.log(result.params, "Inspector issue: ");
    testRunner.completeTest();
})

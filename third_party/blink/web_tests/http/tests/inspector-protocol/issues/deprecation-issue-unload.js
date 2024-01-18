(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that unload deprecation issues are reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate("addEventListener('unload', () => {})");
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

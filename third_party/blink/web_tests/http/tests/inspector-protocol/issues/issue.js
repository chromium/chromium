(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that issues are triggered`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  await session.evaluate('testRunner.triggerTestInspectorIssue()');
  const result = await promise;
  testRunner.log(result.params.issue.code);
  testRunner.completeTest();
})

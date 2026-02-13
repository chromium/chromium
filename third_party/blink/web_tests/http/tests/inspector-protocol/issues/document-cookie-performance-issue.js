(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
 const { session, dp } = await testRunner.startBlank(`Tests that performance issues are reported for document.cookie access`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  // Update this to test different deprecated code if WebFeature::kRangeExpand is removed.
  session.evaluate("document.cookie");
  const result = await promise;

  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
});

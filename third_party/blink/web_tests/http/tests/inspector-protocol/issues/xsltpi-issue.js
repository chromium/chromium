(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
  'Verifies that XSLT Processing Instruction deprecation issue is created');
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  await session.navigate('../resources/xsltpi.xml');
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

(async function (testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Verifies that a deprecation issue is created when a page header includes a deprecated permission policy.\n`);

  await dp.Audits.enable();
  page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/permissions-policy-deprecated.php');
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Inspector issue: ");
  testRunner.completeTest();
})

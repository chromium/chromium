(async function (testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    `Verifies that a deprecation issue is created when a deprecated permission string is used.\n`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate("navigator.permissions.query({ name: 'window-placement' })");
  const issue = await promise;
  testRunner.log(issue.params, "Inspector issue: ");
  testRunner.completeTest();
})

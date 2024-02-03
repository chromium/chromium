(async function (testRunner) {
  const { session, dp } = await testRunner.startBlank(
    'Verifies that using :--foo in CSS creates a deprecation issue.');
  await dp.Audits.enable();

  const issuePromise = dp.Audits.onceIssueAdded();
  session.navigate('../resources/css-custom-state-deprecated-syntax.html');
  const issue = await issuePromise;
  testRunner.log(issue.params, `Issue: `);

  testRunner.completeTest();
})


(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verifies that Expect-CT deprecation issue is created from subresource with Expect-CT header.\n`);

  await dp.Audits.enable();
  page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/expect-ct-subresource.html');
  const issue = await dp.Audits.onceIssueAdded();

  // Record only the issue type because the line number is recorded inconsistently (see https://crbug.com/1345667).
  testRunner.log(issue.params.issue.code, "Inspector issue code: ");
  testRunner.log(issue.params.issue.details.deprecationIssueDetails.type, "Inspector issue type: ");
  testRunner.completeTest();
})

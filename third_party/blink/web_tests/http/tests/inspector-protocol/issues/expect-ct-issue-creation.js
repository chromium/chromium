(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verifies that Expect-CT deprecation issue is created from page with Expect-CT header.\n`);

  await dp.Audits.enable();
  page.navigate('http://127.0.0.1:8000/inspector-protocol/resources/expect-ct.php');
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Inspector issue: ");
  testRunner.completeTest();
})

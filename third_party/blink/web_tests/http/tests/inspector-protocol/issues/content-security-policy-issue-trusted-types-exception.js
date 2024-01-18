(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verify that Trusted Types sink violation issue is generated.\n`);

  await dp.Audits.enable();
  await dp.Runtime.enable();
  page.navigate(
      'https://devtools.test:8443/inspector-protocol/resources/content-security-policy-issue-trusted-types-sink.php');
  const [issue, exception] = await Promise.all(
      [dp.Audits.onceIssueAdded(), dp.Runtime.onceExceptionThrown()]);

  const issueIdFromException =
      exception?.params?.exceptionDetails?.exceptionMetaData?.issueId;
  testRunner.log(`Issue id on issue matches id on exception: ${
      issue.params.issue.issueId === issueIdFromException}`);
  testRunner.completeTest();
})

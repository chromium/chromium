(async function(testRunner) {
  const {dp} = await testRunner.startURL(testRunner.url('../resources/contrast-issue.html'), 'Tests that low text contrast issues are reported.');

  await dp.Audits.enable();

  dp.Audits.checkContrast();

  const issue = await dp.Audits.onceIssueAdded();
  // Round contrast ratio to avoid test failure on different platforms.
  issue.params.issue.details.lowTextContrastIssueDetails.contrastRatio = Number(issue.params.issue.details.lowTextContrastIssueDetails.contrastRatio.toFixed(2));
  testRunner.log(issue.params, "Inspector issue: ");
  testRunner.completeTest();
});

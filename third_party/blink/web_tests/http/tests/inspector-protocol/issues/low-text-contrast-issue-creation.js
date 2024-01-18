(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`<div class="issue" style="color: grey; background-color: white;">text</div>`,
    'Tests that low text contrast issues are reported.');

  await dp.Audits.enable();

  dp.Audits.checkContrast();

  const issue = await dp.Audits.onceIssueAdded();
  // Round contrast ratio to avoid test failure on different platforms.
  const details = issue.params.issue.details.lowTextContrastIssueDetails;
  details.contrastRatio = Number(details.contrastRatio.toFixed(2));
  // details.violatingNodeId might not match exactly depending on the test environment.
  if (!Number.isInteger(details.violatingNodeId)) {
    testRunner.log("Error: violatingNodeId is not an integer.");
  } else {
    details.violatingNodeId = '<integer>';
  }
  testRunner.log(issue.params, "Inspector issue: ");
  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
      `Verifies multiple CSP issues in report-only mode.\n`);

    await dp.Network.enable();
    await dp.Audits.enable();
    page.navigate('https://devtools.test:8443/inspector-protocol/resources/content-security-policy-issue-report-only.php');

    const issues = new Array();
    dp.Audits.onIssueAdded(issue => {
      if (!issue.params.issue.details.contentSecurityPolicyIssueDetails) {
        return;
      }
      issues.push(issue);
      if (issues.length == 3) {
        issues.sort((a, b) => {
          const lineNumberDiff = a.params.issue.details.contentSecurityPolicyIssueDetails.sourceCodeLocation.lineNumber - b.params.issue.details.contentSecurityPolicyIssueDetails.sourceCodeLocation.lineNumber;
          return lineNumberDiff || a.params.issue.details.contentSecurityPolicyIssueDetails.sourceCodeLocation.columnNumber - b.params.issue.details.contentSecurityPolicyIssueDetails.sourceCodeLocation.columnNumber;
        });
        for (const issue of issues) {
          testRunner.log(issue.params, "Inspector issue: ", ["violatingNodeId", "scriptId"]);
        }
        testRunner.completeTest();
      }
    });
  })

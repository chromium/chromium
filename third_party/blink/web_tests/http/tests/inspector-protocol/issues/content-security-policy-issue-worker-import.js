(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that CSP issue is created on the page for a module worker's import blocked by the page's policy.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();
  page.navigate(
      'https://devtools.test:8443/inspector-protocol/resources/content-security-policy-worker-import.php');

  dp.Audits.onIssueAdded(issue => {
    if (!issue.params.issue.details.contentSecurityPolicyIssueDetails) {
      return;
    }
    testRunner.log(issue.params, 'Inspector issue: ');
    testRunner.completeTest();
  });
})

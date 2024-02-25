(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startBlank(
      `Verifies that CSP issue contains source location.\n`);

    await dp.Network.enable();
    await dp.Audits.enable();
    page.navigate('https://devtools.test:8443/inspector-protocol/resources/content-security-policy-issue-with-src-location.php');
    const issue = await dp.Audits.onceIssueAdded();

    testRunner.log(issue.params, "Inspector issue: ");
    testRunner.completeTest();
  })

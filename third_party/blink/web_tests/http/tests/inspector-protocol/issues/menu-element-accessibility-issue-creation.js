(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Verifies that menu accessibility issues are created when the content ' +
      'model is not followed.');
  await dp.Audits.enable();

  const issues = [];
  dp.Audits.onIssueAdded(issue => {
    if (issue.params.issue.code !== 'ElementAccessibilityIssue') {
      return;
    }
    const details = issue.params.issue.details.elementAccessibilityIssueDetails;
    if (!Number.isInteger(details.nodeId)) {
      testRunner.log('Error: nodeId is not an integer.' + details.nodeId);
    }
    issues.push(issue.params);
  });

  await session.navigate(
      '../resources/disallowed-menu-element-descendants.html');

  issues.forEach(issue => testRunner.log(issue, 'Inspector issue: '));
  testRunner.completeTest();
})

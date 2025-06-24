(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { session, dp } = await testRunner.startBlank(
    'Verifies that the <select> accessibility issue is created when the content ' +
    'model is not followed.');
    await dp.Audits.enable();

    let issues = [];
    let count = 0;
    let eventReceived = null;
    let allEventsReceived = new Promise(resolve => eventReceived = () => {
      if (++count == 1) resolve();
    });

    dp.Audits.onceIssueAdded(issue => {
      if (issue.params.issue.code !== 'ElementAccessibilityIssue') {
        return;
      }
      const details = issue.params.issue.details.elementAccessibilityIssueDetails;
      if (!Number.isInteger(details.nodeId)) {
        testRunner.log("Error: nodeId is not an integer." + details.nodeId);
      }
      issues.push(issue.params);
      eventReceived();
    });

    await session.navigate('../resources/disallowed-select-element-descendants.html');
    await allEventsReceived;

    issues.forEach(issue => testRunner.log(issue, "Inspector issue: "));
    testRunner.completeTest();
})

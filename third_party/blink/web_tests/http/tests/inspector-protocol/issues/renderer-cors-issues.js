(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure early CORS issues are correctly reported.`);

  await dp.Audits.enable();
  const issues = [];
  const issuesReceived =
      new Promise(resolve => dp.Audits.onIssueAdded(issue => {
        issues.push(issue.params.issue);
        if (issues.length === 3)
          resolve();
      }));

  await dp.Runtime.enable();
  const issueIdToException = new Map();
  const exceptionsThrown =
      new Promise(resolve => dp.Runtime.onExceptionThrown(exception => {
        const metaData = exception.params.exceptionDetails.exceptionMetaData;
        issueIdToException.set(metaData.issueId, exception.params);
        if (issueIdToException.size === 3)
          resolve();
      }));

  session.evaluate(`
    try {
      fetch('file://doesnt.matter');
    } catch (e) {}

    try {
      fetch('https://devtools-b.oopif.test:8443/index.html', {mode:
    "same-origin"});
    } catch (e) {}

    try {
      fetch('https://devtools-b.oopif.test:8443/index.html', {mode:
    "no-cors", redirect: "error"});
    } catch (e) {}
  `);

  await issuesReceived;
  await exceptionsThrown;

  issues.sort(
      (a, b) =>
          a.details?.corsIssueDetails?.corsErrorStatus?.corsError.localeCompare(
              b.details?.corsIssueDetails?.corsErrorStatus?.corsError));
  for (const issue of issues) {
    testRunner.log(issue, 'Cors issue: ', ['requestId', 'issueId', 'scriptId']);
    testRunner.log(`Issue link present: ${
        Boolean(issueIdToException.get(issue.issueId))}`);
  }
  testRunner.completeTest();
})

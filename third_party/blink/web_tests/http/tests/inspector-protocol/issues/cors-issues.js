(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure CORS issues are correctly reported.`);

  // This url should be cross origin.
  const url = `https://127.0.0.1:8443/inspector-protocol/network/resources`;


  await dp.Audits.enable();
  const issues = [];
  const issuesReceived =
      new Promise(resolve => dp.Audits.onIssueAdded(issue => {
        issues.push(issue.params.issue);
        if (issues.length === 4)
          resolve();
      }));

  await dp.Runtime.enable();
  const issueIdToException = new Map();
  const exceptionsThrown =
      new Promise(resolve => dp.Runtime.onExceptionThrown(exception => {
        const metaData = exception.params.exceptionDetails.exceptionMetaData;
        issueIdToException.set(metaData.issueId, exception.params);
        if (issueIdToException.size === 4)
          resolve();
      }));

  session.evaluate(`
    fetch('${url}/cors-headers.php');

    fetch('${url}/cors-headers.php?origin=${
      encodeURIComponent('http://127.0.0.1')}');

    fetch("${url}/cors-headers.php?methods=GET&origin=1", {method: 'POST',
    mode: 'cors', body: 'FOO', cache: 'no-cache',
    headers: { 'Content-Type': 'application/json'} });

    fetch("${url}/cors-redirect.php");
  `);

  await issuesReceived;
  await exceptionsThrown;

  issues.sort(
      (a, b) =>
          a.details?.corsIssueDetails?.corsErrorStatus?.corsError.localeCompare(
              b.details?.corsIssueDetails?.corsErrorStatus?.corsError));
  for (const issue of issues) {
    testRunner.log(issue, 'Cors issue: ', ['requestId', 'issueId']);
    testRunner.log(`Issue link present: ${
        Boolean(issueIdToException.get(issue.issueId))}`);
  }
  testRunner.completeTest();
})

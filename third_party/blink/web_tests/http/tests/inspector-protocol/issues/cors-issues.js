(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure CORS issues are correctly reported.`);

  // This url should be cross origin.
  const url = `https://127.0.0.1:8443/inspector-protocol/network/resources`;

  await dp.Audits.enable();

  session.evaluate(`
    fetch('${url}/cors-headers.php');

    fetch('${url}/cors-headers.php?origin=${
      encodeURIComponent('http://127.0.0.1')}');

    fetch("${url}/cors-headers.php?methods=GET&origin=1", {method: 'POST',
    mode: 'cors', body: 'FOO', cache: 'no-cache',
    headers: { 'Content-Type': 'application/json'} });

    fetch("${url}/cors-redirect.php");
  `);

  const issues = [];
  for (let i = 0; i < 4; ++i) {
    const issue = await dp.Audits.onceIssueAdded();
    issues.push(issue.params.issue);
  }

  issues.sort(
      (a, b) =>
          a.details?.corsIssueDetails?.corsErrorStatus?.corsError.localeCompare(
              b.details?.corsIssueDetails?.corsErrorStatus?.corsError));
  testRunner.log(issues);
  testRunner.completeTest();
})

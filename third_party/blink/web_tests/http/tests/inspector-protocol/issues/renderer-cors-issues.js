(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure early CORS issues are correctly reported.`);

  await dp.Audits.enable();

  const result = session.evaluate(`
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

  const issues = [];
  for (let i = 0; i < 3; ++i) {
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

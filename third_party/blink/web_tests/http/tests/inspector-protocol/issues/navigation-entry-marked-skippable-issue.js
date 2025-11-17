(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verify that Skippable Navigation Entry issue is generated.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  const issuePromise = dp.Audits.onceIssueAdded();

  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/navigation-entry-marked-skippable.html');

  const issue = await issuePromise;

  testRunner.log(issue.params, "Inspector issue: ", ["affectedRequest"]);

  testRunner.completeTest();
})
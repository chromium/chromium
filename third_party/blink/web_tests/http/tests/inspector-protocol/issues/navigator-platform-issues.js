(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test to make sure navigator.userAgent usage is correctly reported.`);

  await dp.Audits.enable();

  const result = session.evaluate(`
      console.log(navigator.platform);
  `);

  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue);
  testRunner.completeTest();
})

(async function (testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that NavigateEventRestoreScroll deprecation issue is reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate("navigation.onnavigate = e => { e.intercept(); e.restoreScroll(); }; navigation.navigate('#');");
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

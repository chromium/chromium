(async function (testRunner) {
  const { session, dp } = await testRunner.startBlank(
  'Verifies that Non-streaming declarative shadow DOM deprecation issue is created from page containing <template shadowroot>.');
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();

  await session.navigate('../resources/non-streaming-declarative-shadow-dom.html');

  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

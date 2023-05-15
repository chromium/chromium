(async function (testRunner) {
  const { session, dp } = await testRunner.startBlank(
  'Verifies that Streaming declarative shadow DOM (<template shadowrootmode>) does not trigger a deprecation issue.');
  await dp.Audits.enable();
  const issueAdded = dp.Audits.onceIssueAdded();

  await session.navigate('../resources/streaming-declarative-shadow-dom.html');

  // This is a test that issues are NOT reported. So we need to just wait
  // for a period of time.
  let timedOut = false;
  const timeout = new Promise(resolve => setTimeout(() => {
    timedOut = true;
    resolve();
  }, 100));
  await Promise.race([
    timeout,
    issueAdded,
  ]);
  testRunner.log(timedOut ? "Success - no issues reported" : "FAIL: issues reported.");
  testRunner.completeTest();
})

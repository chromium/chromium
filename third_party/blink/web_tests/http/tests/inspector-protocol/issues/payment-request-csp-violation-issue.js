(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
    'Tests that deprecation issues are reported for ' +
    'bypassing Content-Security-Policy (CSP) in Web Payment API.');
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();

  await session.navigate('../resources/payment-request-csp-violation.html');

  const result = await promise;
  testRunner.log(result.params, 'Inspector issue: ');
  testRunner.completeTest();
})

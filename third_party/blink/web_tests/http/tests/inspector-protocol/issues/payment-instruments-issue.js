(async function (testRunner) {
  const {session, dp} = await testRunner.startBlank(
    'Tests that deprecation issues are reported for ' +
    'paymentManager.instruments');
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  const dir = '/inspector-protocol/resources';

  // Access the deprecated PaymentInstruments field:
  session.evaluate(`navigator.serviceWorker.register("${dir}/blank.js")
    .then((registration) => {
      registration.paymentManager.instruments.clear();
    })`);

  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

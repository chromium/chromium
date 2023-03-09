(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank(
          "Check that the dialogShown event works after enabling the " +
          "FedCm domain");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html");

  await dp.FedCm.enable();

  const result = await session.evaluateAsync("triggerDialog()");
  testRunner.log(result);
  let msg = await dp.FedCm.onceDialogShown();
  if (msg.error) {
    testRunner.log(msg.error);
  } else {
    testRunner.log(msg.params.accounts, "accounts: ");
  }
  testRunner.completeTest();
})

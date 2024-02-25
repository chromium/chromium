(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the cooldown APIs work with FedCM");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html");

  await dp.FedCm.enable({disableRejectionDelay: true});

  const dialogPromise = session.evaluateAsync("triggerDialog()");
  const msg = await dp.FedCm.onceDialogShown();
  dp.FedCm.dismissDialog({dialogId: msg.params.dialogId, triggerCooldown: true});
  // This should be a NetworkError
  testRunner.log(await dialogPromise);

  // This should be auto-dismissed because it is on cooldown.
  testRunner.log(await session.evaluateAsync("triggerDialog()"));

  await dp.FedCm.resetCooldown();
  const dialogPromise2 = session.evaluateAsync("triggerDialog()");
  const msg2 = await dp.FedCm.onceDialogShown();
  dp.FedCm.selectAccount({dialogId: msg2.params.dialogId, accountIndex: 0});
  testRunner.log(await dialogPromise2);
  testRunner.completeTest();
})

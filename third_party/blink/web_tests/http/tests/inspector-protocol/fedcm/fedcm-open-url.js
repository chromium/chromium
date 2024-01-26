(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that opening account URLs works");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html");

  // Enable FedCM domain
  await dp.FedCm.enable({disableRejectionDelay: true});

  // Trigger FedCM dialog
  const dialogPromise = session.evaluateAsync("triggerDialog()");

  let msg = await dp.FedCm.onceDialogShown();
  if (msg.error) {
    testRunner.fail(msg.error);
    return;
  }

  testRunner.log(msg.params, "msg.params: ", ["dialogId"]);

  await dp.Target.setDiscoverTargets({discover: true});
  // This should fail because this account has no URLs.
  let result = await dp.FedCm.openUrl({dialogId: msg.params.dialogId,
                                       accountIndex: 0,
                                       accountUrlType: "TermsOfService"});
  testRunner.log(result.error);

  // This should succeed.
  dp.FedCm.openUrl({dialogId: msg.params.dialogId,
                    accountIndex: 1,
                    accountUrlType: "TermsOfService"});

  await dp.Target.onceTargetCreated();
  const pageTargets = (await dp.Target.getTargets()).result.targetInfos.filter(target => target.type === 'page');
  testRunner.log("Opened URL: " + pageTargets[0].url);

  dp.FedCm.dismissDialog({dialogId: msg.params.dialogId});
  testRunner.completeTest();
})

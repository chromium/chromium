(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          "Check that the dialogShown event works after enabling the " +
          "FedCm domain then triggering the FedCm dialog");

  // Mark IDP signin status as signed in. This has to be a toplevel load.
  await page.navigate("https://127.0.0.1:8443/resources/fedcm/mark-signin.php");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event-with-iss-ot.html");

  // Make sure we get no accounts so that the confirm IDP signin dialog gets
  // triggered.
  await fetch(
      "https://127.0.0.1:8443/resources/fedcm/set-accounts.php?noaccounts",
      {crossorigin: true, credentials: "include"});

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
  if (msg.params.dialogType != "ConfirmIdpSignin") {
    testRunner.fail("Wrong dialog type");
    return;
  }
  dp.FedCm.confirmIdpSignin({dialogId: msg.params.dialogId});

  // Now wait for the account chooser dialog.
  msg = await dp.FedCm.onceDialogShown();
  testRunner.log(msg.params, "msg.params: ", ["dialogId"]);
  dp.FedCm.selectAccount({dialogId: msg.params.dialogId, accountIndex: 0});
  const token = await dialogPromise;
  testRunner.log("token: " + token);
  testRunner.completeTest();
})

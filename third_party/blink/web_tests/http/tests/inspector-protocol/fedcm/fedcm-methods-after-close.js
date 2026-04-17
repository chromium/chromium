(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Check that FedCm methods fail after dialog close');

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html');

  await dp.FedCm.enable({disableRejectionDelay: true});

  const dialogPromise = session.evaluateAsync('triggerDialog()');
  const msg = await dp.FedCm.onceDialogShown();
  const dialogId = msg.params.dialogId;
  dp.FedCm.dismissDialog({dialogId: dialogId});
  await dp.FedCm.onceDialogClosed();

  async function testMethod(name, promise) {
    const response = await promise;
    testRunner.log(`${name} response error: ${response.error.message}`);
  }

  await testMethod(
      'selectAccount',
      dp.FedCm.selectAccount({dialogId: dialogId, accountIndex: 0}));
  await testMethod('openUrl', dp.FedCm.openUrl({
    dialogId: dialogId,
    accountIndex: 0,
    accountUrlType: 'TermsOfService'
  }));
  await testMethod(
      'clickDialogButton',
      dp.FedCm.clickDialogButton(
          {dialogId: dialogId, dialogButton: 'ConfirmIdpLoginContinue'}));
  await testMethod(
      'dismissDialog', dp.FedCm.dismissDialog({dialogId: dialogId}));

  testRunner.log(await dialogPromise);
  testRunner.completeTest();
})

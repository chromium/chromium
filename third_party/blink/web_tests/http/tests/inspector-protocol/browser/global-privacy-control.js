(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test global permission control state can be read and written`);

  // Browser target has access.
  let result = await testRunner.browserP().Browser.getGlobalPrivacyControl();
  testRunner.log('-Initial State- ' + result.result.gpc);
  result =
      await testRunner.browserP().Browser.setGlobalPrivacyControl({gpc: true});
  testRunner.log('-Updated True State- ' + result.result.gpc);
  await page.navigate(testRunner.url('../resources/empty.html'));
  result = await testRunner.browserP().Browser.getGlobalPrivacyControl();
  testRunner.log('-Post Navigation State- ' + result.result.gpc);
  result =
      await testRunner.browserP().Browser.setGlobalPrivacyControl({gpc: false});
  testRunner.log('-Updated False State- ' + result.result.gpc);
  await dp.Page.reload();
  result = await testRunner.browserP().Browser.getGlobalPrivacyControl();
  testRunner.log('-Post Reload State- ' + result.result.gpc);

  // Page target lacks access.
  result = await dp.Browser.getGlobalPrivacyControl();
  testRunner.log('-Page Get Error- ' + result.error.message);
  result = await dp.Browser.setGlobalPrivacyControl({gpc: true});
  testRunner.log('-Page Set Error- ' + result.error.message);

  // Disconnecting a browser session (but not a page session) resets the state.
  let sessionB = await testRunner.attachFullBrowserSession();
  let sessionP = await page.createSession();
  result = await sessionB.protocol.Browser.setGlobalPrivacyControl({gpc: true});
  testRunner.log('-Second Browser Session State After Update- ' +
                 result.result.gpc);
  result = await testRunner.browserP().Browser.getGlobalPrivacyControl();
  testRunner.log('-First Browser Session State After Update- ' +
                 result.result.gpc);
  await sessionP.disconnect();
  result = await testRunner.browserP().Browser.getGlobalPrivacyControl();
  testRunner.log(
      '-First Browser Session State After Second Page Session Disconnect- ' +
      result.result.gpc);
  await sessionB.disconnect();
  result = await testRunner.browserP().Browser.getGlobalPrivacyControl();
  testRunner.log(
      '-First Browser Session State After Second Browser Session Disconnect- ' +
      result.result.gpc);

  testRunner.completeTest();
});

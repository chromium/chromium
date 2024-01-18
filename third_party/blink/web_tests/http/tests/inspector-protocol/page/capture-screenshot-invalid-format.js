(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that calling Page.captureScreenshot with invalid format returns an error`);

  const response = await dp.Page.captureScreenshot({format: "pcx"});
  testRunner.log(response.error || 'FAIL: error not reported');
  testRunner.completeTest();
})

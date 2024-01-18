(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, dp} = await testRunner.startBlank(
      `Tests ReportingApiEndpointsChangedForOrigin events.\n`);
  await dp.Network.enable();

  dp.Network.onReportingApiEndpointsChangedForOrigin(event => {
    testRunner.log('Endpoints updated:')
    testRunner.log(event.params);
    testRunner.completeTest();
  });

  await page.navigate(testRunner.url('https://127.0.0.1:8443/inspector-protocol/network/resources/generate-report.php'))
  await dp.Network.enableReportingApi({enable: true});
})

(async function(testRunner) {
  var {page, dp} = await testRunner.startBlank(
      `Tests ReportingApiReportAdded event.\n`);
  await dp.Network.enable();

  dp.Network.onceReportingApiReportAdded(event => {
    testRunner.log(event.params.report);
    testRunner.completeTest();
  });

  await page.navigate(testRunner.url('resources/generate-report.php'));
  await dp.Network.enableReportingApi({enable: true});
})

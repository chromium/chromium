(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, dp} = await testRunner.startBlank(
      `Tests ReportingApiReportAdded and ReportingApiReportUpdated events.\n`);
  await dp.Network.enable();
  let count = 0;

  dp.Network.onReportingApiReportAdded(event => {
    testRunner.log('Report added:')
    testRunner.log(event.params.report);
  });

  dp.Network.onReportingApiReportUpdated(event => {
    testRunner.log('Report updated:')
    testRunner.log(event.params.report);
    count++;
    if (count === 2) testRunner.completeTest();
  });

  await page.navigate(testRunner.url(
      'http://localhost:8080/inspector-protocol/network/resources/generate-report.php'));
  await dp.Network.enableReportingApi({enable: true});
})

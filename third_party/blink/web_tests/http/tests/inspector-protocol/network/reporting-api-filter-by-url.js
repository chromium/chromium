(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var eventCount = 0;
  var {page, dp} = await testRunner.startBlank(
      `Tests filtering of ReportingApiReportAdded events (Session 1).\n`);
  await dp.Network.enable();

  var {page: page2, dp: dp2} = await testRunner.startBlank(
    `Tests filtering of ReportingApiReportAdded events (Session 2).\n`);
  await dp2.Network.enable();

  dp.Network.onReportingApiReportAdded(event => {
    testRunner.log('Session 1:');
    testRunner.log(event.params.report);
    eventCount++;
    if (eventCount == 2) {
      testRunner.completeTest();
    }
  });

  dp2.Network.onReportingApiReportAdded(event => {
    testRunner.log('Session 2:');
    testRunner.log(event.params.report);
    eventCount++;
    if (eventCount == 2) {
      testRunner.completeTest();
    }
  });

  // first report is generated and stored
  await page.navigate(testRunner.url('resources/generate-report.php'));

  await page2.navigate(testRunner.url('resources/hello-world.html'));

  // stored report is sent
  await dp.Network.enableReportingApi({enable: true});

  // stored report does not pass filter
  await dp2.Network.enableReportingApi({enable: true});

  // second report is generated and sent to first session only
  await page.navigate(testRunner.url('resources/generate-report.php'));

  await page.navigate(testRunner.url('resources/hello-world.html'));
})

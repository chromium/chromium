(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <div style="color: #666; background-color: white;">AAA issue only</div>
    <div style="color: white; background-color: white;">AA & AAA issue</div>
  `, 'Tests that low text contrast issues are reported when reportAAA option is on and off.');

  await dp.Audits.enable();

  let issues = [];
  dp.Audits.onIssueAdded(issue => issues.push(issue));

  await dp.Audits.checkContrast({
    reportAAA: false,
  });

  testRunner.log('Number of issues created with !reportAAA: ' + issues.length);

  issues = [];

  await dp.Audits.checkContrast({
    reportAAA: true,
  });

  testRunner.log('Number of issues created with reportAAA: ' + issues.length);
  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
    `Verifies that the AffectedRequest is included in an mixed content issue created when a HTTPS url redirects to a HTTP script (crbug.com/1190808)\n`);

  await dp.Audits.enable();
  await dp.Network.enable();
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/empty.html');

  const issuePromise = dp.Audits.onceIssueAdded();
  await page.loadHTML(`
    <script src='https://devtools.test:8443/inspector-protocol/resources/redirect-mixed-content-script.php'></script>
  `)
  const issue = await issuePromise;
  // TODO(chromium:1190808): The test doesn't currently output the requestId but it actually should.
  //     Remove this TODO and rebaseline the test once the bug is fixed.
  testRunner.log(issue.params, "Inspector issue: ", ["frameId", "requestId"]);
  testRunner.completeTest();
})

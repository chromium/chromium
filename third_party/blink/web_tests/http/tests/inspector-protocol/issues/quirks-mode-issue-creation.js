(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, page} = await testRunner.startBlank('Tests that Quirks Mode issues are reported.');

  await dp.Audits.enable();

  let expectedIssues = 3;
  dp.Audits.onIssueAdded(issue => {
    testRunner.log(issue.params, "Inspector issue: ", ['documentNodeId', 'frameId', 'loaderId']);
    if (--expectedIssues === 0) {
      testRunner.completeTest();
    }
  });

  // Test that going to a no-quirks page does not report QuirksMode issues.
  await page.navigate('../resources/inspector-protocol-page.html');

  await page.navigate('../resources/quirks-mode.html');

  await page.navigate('../resources/limited-quirks-mode.html');

  await page.navigate('../resources/quirks-mode-in-iframe.html');
});

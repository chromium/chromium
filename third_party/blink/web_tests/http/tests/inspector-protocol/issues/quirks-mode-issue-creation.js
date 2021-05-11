(async function(testRunner) {
  const {dp, page} = await testRunner.startBlank('Tests that Quirks Mode issues are reported.');

  await dp.Audits.enable();

  dp.Audits.onIssueAdded(issue => {
    testRunner.log(issue.params, "Inspector issue: ");
  });

  await page.navigate('../resources/quirks-mode.html');

  // Test that going to a no-quirks page does not report QuirksMode issues.
  await page.navigate('../resources/inspector-protocol-page.html');

  await page.navigate('../resources/limited-quirks-mode.html');

  await page.navigate('../resources/quirks-mode-in-iframe.html');

  testRunner.completeTest();
});

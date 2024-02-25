(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that execution contexts are reported for frames that were blocked due to mixed content when Runtime is enabled *after* navigation.`);
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content-iframe.html');
  dp.Runtime.onExecutionContextCreated(event => {
    // TODO(caseq): remove following v8 roll past https://chromium-review.googlesource.com/c/v8/v8/+/2594538
    delete event.params.context.uniqueId;

    testRunner.log(event);
  });
  await dp.Runtime.enable();
  testRunner.completeTest();
})

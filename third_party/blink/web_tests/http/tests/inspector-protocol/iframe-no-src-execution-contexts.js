(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that execution contexts are reported for iframes that don't have src attribute.`);
  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/iframe-no-src.html');
  dp.Runtime.onExecutionContextCreated(event => {
    // TODO(caseq): remove following v8 roll past https://chromium-review.googlesource.com/c/v8/v8/+/2594538
    delete event.params.context.uniqueId;
    testRunner.log(event);
  });
  await dp.Runtime.enable();
  testRunner.completeTest();
})

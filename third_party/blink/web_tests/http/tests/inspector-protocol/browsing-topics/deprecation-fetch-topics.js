(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const baseOrigin = 'https://a.test:8443/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  const {page, session, dp} = await testRunner.startBlank(
      `Tests that deprecation issues are reported for fetch(url, {browsingTopics: true}).\n`);

  await dp.Audits.enable();

  async function evaluateAndLogIssue(js) {
    // Unmute deprecation issue reporting by navigating.
    await page.navigate(base + 'empty.html');
    let promise = dp.Audits.onceIssueAdded();
    await session.evaluateAsync(js);
    let result = await promise;
    testRunner.log(result.params, 'Inspector issue: ');
  }

  await evaluateAndLogIssue("fetch('empty.html', {browsingTopics: true})");

  testRunner.completeTest();
})

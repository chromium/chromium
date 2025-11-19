(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const baseOrigin = 'https://a.test:8443/';
  const base = baseOrigin + 'inspector-protocol/resources/';

  const {page, session, dp} = await testRunner.startBlank(
      `Tests that deprecation issues are reported for <img browsingtopics>.\n`);

  await dp.Audits.enable();

  async function evaluateAndLogIssue() {
    // Unmute deprecation issue reporting by navigating.
    await page.navigate(base + 'empty.html');
    let promise = dp.Audits.onceIssueAdded();

    // We trigger the issue by creating an img, setting the property,
    // and appending it to the DOM to start the fetch.
    await session.evaluate((url) => {
      const img = document.createElement('img');
      img.browsingTopics = true;
      img.src = url;
      document.body.appendChild(img);
    }, base + '1x.png');

    let result = await promise;
    testRunner.log(result.params, 'Inspector issue: ');
  }

  await evaluateAndLogIssue();

  testRunner.completeTest();
})

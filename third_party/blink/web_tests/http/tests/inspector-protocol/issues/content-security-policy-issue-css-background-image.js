(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that CSP issue for an image fetched by CSS has no source code location.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();
  await dp.Page.enable();
  page.navigate(
      'https://devtools.test:8443/inspector-protocol/resources/content-security-policy-issue-css-background-image.php');
  await dp.Page.onceLoadEventFired();

  session.evaluate('document.getElementById("target").className = "blocked"');
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, 'Inspector issue: ');
  testRunner.completeTest();
})

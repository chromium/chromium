(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verifies that mixed content issue is created from mixed content form.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();
  page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content-form.html');
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Inspector issue: ");
  testRunner.completeTest();
})

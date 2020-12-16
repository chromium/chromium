(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/resources/empty.html',
      `Verifies that post-messaging a SAB causes an issue.\n`);

  await dp.Audits.enable();
  session.evaluate(`postMessage(new SharedArrayBuffer());`);
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, 'Inspector issue: ');
  testRunner.completeTest();
})

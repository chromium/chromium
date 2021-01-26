(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/resources/empty.html',
      `Verifies that post-messaging a SAB causes an issue.\n`);

  await dp.Audits.enable();
  session.evaluate(`postMessage(new SharedArrayBuffer());`);
  const issues = await Promise.all(
      [dp.Audits.onceIssueAdded(), dp.Audits.onceIssueAdded()]);

  testRunner.log(issues[0].params, 'Creation issue: ');
  testRunner.log(issues[1].params, 'Transfer issue: ');
  testRunner.completeTest();
})

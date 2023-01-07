(async function(testRunner) {
  const {session, dp} = await testRunner.startURL(
      'http://devtools.test:8000/inspector-protocol/resources/empty.html',
      `Tests that deprecation issues are reported`);

  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate('window.openDatabase(\'testdb\', \'1.0\', \'\', 1)');

  const result = await promise;
  testRunner.log(result.params, 'Inspector issue: ');
  testRunner.completeTest();
})

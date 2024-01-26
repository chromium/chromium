(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'http://devtools.test:8000/inspector-protocol/resources/empty.html',
      `Tests that deprecation issues are reported`);

  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate('openDatabase("testdb", "1", "", 1024)');

  const result = await promise;
  testRunner.log(result.params, 'Inspector issue: ');
  testRunner.completeTest();
})

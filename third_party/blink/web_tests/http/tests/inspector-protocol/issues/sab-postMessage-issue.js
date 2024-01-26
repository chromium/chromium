(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/resources/empty.html',
      `Verifies that post-messaging a SAB causes an issue.\n`);

  await dp.Audits.enable();
  session.evaluate(`postMessage(new (new WebAssembly.Memory(
         { shared:true, initial:0, maximum:0 }).buffer.constructor)());`);
  const issue1 = await dp.Audits.onceIssueAdded();
  const issue2 = await dp.Audits.onceIssueAdded();
  const issue3 = await dp.Audits.onceIssueAdded();
  testRunner.log(issue1.params, 'Creation issue: ');
  testRunner.log(issue2.params, 'Deprecation issue: ');
  testRunner.log(issue3.params, 'Transfer issue: ');
  testRunner.completeTest();
})

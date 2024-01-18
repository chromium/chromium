(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/resources/empty.html',
      `Verifies that creating a SAB in a non-COI context causes an issue.\n`);

  await dp.Audits.enable();
  session.evaluate(
      `var x = new (new WebAssembly.Memory(
         { shared:true, initial:0, maximum:0 }).buffer.constructor)();`);
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, 'Inspector issue: ');
  testRunner.completeTest();
})

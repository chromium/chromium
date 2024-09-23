(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // TODO(crbug.com/41492947) This test can be deleted once getInnerHTML is removed.
  const { session, dp } = await testRunner.startBlank(
  'Verifies that calling getInnerHTML() triggers a deprecation issue.');

  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate('document.body.getInnerHTML()');

  let noIssue = false;
  const timeout = new Promise(resolve => setTimeout(() => {noIssue=true; resolve();},500));
  const result = await Promise.any([promise, timeout]);
  if (noIssue) {
    testRunner.log('No issue');
  } else {
    testRunner.log(result.params, 'Inspector issue: ');
  }
  testRunner.completeTest();
})

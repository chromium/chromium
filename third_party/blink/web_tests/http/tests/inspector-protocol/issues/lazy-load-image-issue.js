(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that lazy-load image zero-size issues are reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate(`
    const img = document.createElement('img');
    img.src = 'data:image/png,';
    img.loading = 'lazy';
    document.body.appendChild(img);
  `);
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})

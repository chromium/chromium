(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // TODO(crbug.com/394111284) If/when the special H1 font rules are removed, this test can be deleted.
  const { session, dp } = await testRunner.startBlank(
  'Verifies that H1 within section (without font overrides) triggers a deprecation issue.');

  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate(`
    const section = document.createElement('section');
    const h1 = document.createElement('h1');
    section.appendChild(h1);
    document.body.appendChild(section);
  `);

  const result = await promise;
  testRunner.log(result.params, 'Inspector issue: ');
  testRunner.completeTest();
})



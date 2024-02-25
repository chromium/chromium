// TODO(crbug.com/1426629) This test can be deleted once non-standard CSS appearance value slider-vertical is removed.
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that non-standard appearance value slider-vertical trigger a deprecation issue using CSSOM setProperty.`);
  await dp.Audits.enable();

  const waitForIssue = async (prop, value) => {
    const promise = dp.Audits.onceIssueAdded();
    await session.navigate(`../resources/css-appearance-non-standard-cssom.html?${prop}&${value}`);
    let noIssue = false;
    const wait2frames = new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(() => {noIssue=true; resolve();})));
    const result = await Promise.any([wait2frames, promise]);

    if (noIssue) {
      testRunner.log(`${prop} value "${value}" did not generate issue.`);
    } else {
      testRunner.log(result.params, `Issue for ${prop} "${value}": `);
    }
  }

  // invalid value should show warning
  await waitForIssue('appearance', 'slider-vertical');
  await waitForIssue('-webkit-appearance', 'slider-vertical');
  // valid value should not show warning
  await waitForIssue('appearance', 'auto');
  await waitForIssue('-webkit-appearance', 'auto');

  testRunner.completeTest();
})

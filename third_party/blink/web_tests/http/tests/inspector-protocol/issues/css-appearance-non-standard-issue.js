// TODO(crbug.com/924486) This test can be deleted once non-standard CSS appearance values are removed.
(async function (testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that non-standard appearance values trigger a deprecation issue.`);
  await dp.Audits.enable();

  const waitForIssue = async (prop, value) => {
    const promise = dp.Audits.onceIssueAdded();
    await session.navigate(`../resources/css-appearance-non-standard.html?${prop}&${value}`);
    let noIssue = false;
    const wait2frames = new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(() => {noIssue=true; resolve();})));
    const result = await Promise.any([wait2frames, promise]);

    if (noIssue) {
      testRunner.log(`${prop} value "${value}" did not generate issue.`);
    } else {
      testRunner.log(result.params, `Issue for ${prop} "${value}": `);
    }
  }

  const values = [
    // invalid values
    'inner-spin-button',
    'media-slider',
    'media-sliderthumb',
    'media-volume-slider',
    'media-volume-sliderthumb',
    'push-button',
    'searchfield-cancel-button',
    'slider-horizontal',
    'sliderthumb-horizontal',
    'sliderthumb-vertical',
    'square-button',
    // valid values
    'auto',
    'none',
  ];

  for (const value of values) {
    await waitForIssue('appearance', value);
  }

  testRunner.completeTest();
})

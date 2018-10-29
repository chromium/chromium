(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Test that CSS coverage works for a newly added stylesheet.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  await dp.CSS.startRuleUsageTracking();
  await session.evaluateAsync(async function(url) {
    const div = document.createElement('div');
    div.classList.add('usedAtTheVeryEnd');
    document.body.appendChild(div);

    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = url;
    document.head.appendChild(link);
    await new Promise(fulfill => link.onload = fulfill);
  }, testRunner.url('./resources/coverage2.css'));
  const response = await dp.CSS.stopRuleUsageTracking();

  if (response.result.ruleUsage.length === 1) {
    testRunner.log('Successfully reported CSS coverage.')
  } else {
    testRunner.log(`ERROR!`);
    testRunner.log(response.result.ruleUsage);
  }
  testRunner.completeTest();
});

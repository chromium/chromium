(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that a feature policy header parse error can log during navigation commit without crashing.`);

  const log = [];
  await dp.Log.enable();
  dp.Log.onEntryAdded((event) => { testRunner.log(event.params.entry.text); });
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/feature-policy-parse-error.php');

  testRunner.completeTest();
})

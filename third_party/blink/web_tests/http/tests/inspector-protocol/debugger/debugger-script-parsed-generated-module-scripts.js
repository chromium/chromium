(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Tests that dynamically created module scripts don't have a sourceURL and embedderName.`);

  dp.Debugger.enable();

  page.navigate('https://devtools.test:8443/inspector-protocol/resources/dynamic-module-script.html');

  testRunner.log(await dp.Debugger.onceScriptParsed());
  testRunner.log(await dp.Debugger.onceScriptParsed());

  testRunner.completeTest();
});

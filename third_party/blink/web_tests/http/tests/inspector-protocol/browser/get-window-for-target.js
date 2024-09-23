(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startBlank('Verifies Browser.getWindowForTarget method.');
  const response = await dp.Browser.getWindowForTarget();
  testRunner.log(response.result, 'Response', ['left', 'top', 'width', 'height']);
  testRunner.completeTest();
})

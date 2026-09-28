(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that FindInPage.findNext fails without an active find session.');

  const response = await dp.FindInPage.findNext();
  testRunner.log(`Command failed: ${!!response.error}`);
  testRunner.log(`Error code: ${response.error.code}`);

  testRunner.completeTest();
})

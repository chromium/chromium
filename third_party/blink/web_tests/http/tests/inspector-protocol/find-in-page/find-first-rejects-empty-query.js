(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that FindInPage.findFirst rejects an empty query.');

  const response = await dp.FindInPage.findFirst({query: ''});
  testRunner.log(`Command failed: ${!!response.error}`);
  testRunner.log(`Error code: ${response.error.code}`);

  testRunner.completeTest();
})

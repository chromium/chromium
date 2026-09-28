(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that FindInPage.stop ends the session so a subsequent findNext fails.');

  await dp.FindInPage.findFirst({query: 'hello'});
  const stopResponse = await dp.FindInPage.stop();
  testRunner.log(`stop succeeded: ${!stopResponse.error}`);

  const findNextResponse = await dp.FindInPage.findNext();
  testRunner.log(`findNext after stop failed: ${!!findNextResponse.error}`);

  testRunner.completeTest();
})

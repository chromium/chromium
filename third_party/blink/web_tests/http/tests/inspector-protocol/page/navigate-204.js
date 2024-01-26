(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.navigate returns error when server responds with HTTP 204.`);

  await dp.Page.enable();
  const response = await dp.Page.navigate({ url: testRunner.url('./resources/http204.php')});
  testRunner.log(response);
  testRunner.completeTest();
})

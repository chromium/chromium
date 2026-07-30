(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the browser does not crash when an invalid request stage is requested in Fetch.enable`);

  await dp.Page.enable();

  const result = await dp.Fetch.enable(
      {patterns: [{urlPattern: '*', requestStage: 'FOO'}]});
  testRunner.log(result);

  testRunner.completeTest();
})

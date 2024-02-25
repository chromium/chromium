(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests the browser does not crash when an invalid interception stage is requested (crbug.com/1500377)`);

  session.protocol.Network.enable();
  session.protocol.Page.enable();

  const result = await session.protocol.Network.setRequestInterception(
      {patterns: [{urlPattern: '*', interceptionStage: 'FOO'}]});
  testRunner.log(result);

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test that profiler doesn\'t crash when we call stop without preceeding start.');
  var messageObject = await dp.Profiler.stop();
  testRunner.expectedError('ProfileAgent.stop', messageObject);
  testRunner.completeTest();
})

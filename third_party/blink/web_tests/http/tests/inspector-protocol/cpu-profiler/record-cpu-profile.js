(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test that profiler is able to record a profile. ' +
      'Also it tests that profiler returns an error when it unable to find the profile.');

  dp.Profiler.enable();
  var messageObject = await dp.Profiler.start();
  if (!testRunner.expectedSuccess('startFrontendProfile', messageObject))
    return;

  messageObject = await dp.Runtime.evaluate({expression: 'console.profile("Profile 1");'});
  if (!testRunner.expectedSuccess('startConsoleProfile', messageObject))
    return;

  messageObject = await dp.Runtime.evaluate({expression: 'console.profileEnd("Profile 1");'});
  if (!testRunner.expectedSuccess('stopConsoleProfile', messageObject))
    return;

  messageObject = await dp.Profiler.stop();
  if (!testRunner.expectedSuccess('stoppedFrontendProfile', messageObject))
    return;

  messageObject = await dp.Profiler.start();
  if (!testRunner.expectedSuccess('startFrontendProfileSecondTime', messageObject))
    return;

  messageObject = await dp.Profiler.stop();
  testRunner.expectedSuccess('stopFrontendProfileSecondTime', messageObject);
  testRunner.completeTest();
})

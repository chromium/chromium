(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test that profiling can only be started when Profiler was enabled and that ' +
      'Profiler.disable command will stop recording all profiles.');

  var allowConsoleProfiles = false;
  dp.Profiler.onConsoleProfileStarted(messageObject => {
    if (allowConsoleProfiles)
      testRunner.log('PASS: console initiated profile started')
    else
      testRunner.log('FAIL: console profile started ' + JSON.stringify(messageObject, null, 4));
  });
  dp.Profiler.onConsoleProfileFinished(messageObject => {
    if (allowConsoleProfiles)
      testRunner.log('PASS: console initiated profile received')
    else
      testRunner.log('FAIL: unexpected profile received ' + JSON.stringify(messageObject, null, 4));
  });

  var messageObject = await dp.Profiler.start();
  if (!testRunner.expectedError('didFailToStartWhenDisabled', messageObject))
    return;

  allowConsoleProfiles = true;
  dp.Profiler.enable();
  messageObject = await dp.Profiler.start();
  if (!testRunner.expectedSuccess('didStartFrontendProfile', messageObject))
    return;

  messageObject = await dp.Runtime.evaluate({expression: 'console.profile("p1");'});
  if (!testRunner.expectedSuccess('didStartConsoleProfile', messageObject))
    return;

  messageObject = await dp.Profiler.disable();
  if (!testRunner.expectedSuccess('didDisableProfiler', messageObject))
    return;

  dp.Profiler.enable();
  messageObject = await dp.Profiler.stop();
  if (!testRunner.expectedError('no front-end initiated profiles found', messageObject))
    return;
  allowConsoleProfiles = false;

  messageObject = await dp.Runtime.evaluate({expression: 'console.profileEnd();'});
  if (!testRunner.expectedSuccess('didStopConsoleProfile', messageObject))
    return;
  testRunner.completeTest();
})

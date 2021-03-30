(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that console.profile/profileEnd will record CPU profile when inspector front-end is connected.');

  function fail(message) {
    testRunner.log('FAIL: ' + message);
    testRunner.completeTest();
  }

  function findFunctionInProfile(nodes, functionName) {
    return nodes.some(n => n.callFrame.functionName === functionName);
  }

  var headers = [];
  dp.Profiler.onConsoleProfileFinished(messageObject => {
    headers.push({profile: messageObject['params']['profile'], title: messageObject['params']['title']});
  });

  dp.Profiler.enable();
  await session.evaluate(`
    (function collectProfiles() {
      console.profile('outer');
      console.profile(42);
      console.profileEnd('outer');
      console.profileEnd(42);
    })();
  `);

  if (headers.length !== 2)
    return fail('Cannot retrive headers: ' + JSON.stringify(messageObject, null, 4));

  for (var header of headers) {
    if (header.title === '42') {
      testRunner.log('SUCCESS: retrieved "42" profile');
      if (!findFunctionInProfile(header.profile.nodes, 'collectProfiles'))
        return fail('collectProfiles function not found in the profile: ' + JSON.stringify(header.profile, null, 4));
      testRunner.log('SUCCESS: found "collectProfiles" function in the profile');
      testRunner.completeTest();
      return;
    }
  }

  fail('Cannot find "42" profile header');
})

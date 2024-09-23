(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // <a href='https://bugs.webkit.org/show_bug.cgi?id=105759'>Bug 105759.</a>
  var {page, session, dp} = await testRunner.startBlank('Tests that "console.profileEnd()" does not cause crash.\nBug 105759.');

  function fail(message) {
    testRunner.log('FAIL: ' + message);
    testRunner.completeTest();
  }

  var headers = [];
  dp.Profiler.onConsoleProfileFinished(messageObject => {
    headers.push({title: messageObject['params']['title']});
  });

  dp.Profiler.enable();
  await session.evaluate(`
    (function collectProfiles() {
      console.profile();
      console.profile('titled');
      console.profileEnd('titled');
      console.profileEnd();
    })();
  `);


  if (headers.length !== 2)
    return fail('Cannot retrive headers: ' + JSON.stringify(messageObject, null, 4));

  testRunner.log('SUCCESS: found 2 profile headers');
  for (var header of headers) {
    if (header.title === 'titled') {
      testRunner.log('SUCCESS: titled profile found');
      testRunner.completeTest();
      return;
    }
  }
  fail('Cannot find titled profile');
})

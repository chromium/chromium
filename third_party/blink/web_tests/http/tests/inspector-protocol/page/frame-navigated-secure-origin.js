(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that Page.frameNavigated reports isSecureOrigin[Explanation] correctly');

  await dp.Page.enable();

  function onFrameNavigated(event) {
    const frame = event.params.frame;
    testRunner.log(JSON.stringify(frame, ["securityOrigin", "secureContextType"], 2));
  }
  dp.Page.onFrameNavigated(onFrameNavigated);

  dp.Target.onAttachedToTarget(async e => {
    const dp2 = session.createChild(e.params.sessionId).protocol;
    await dp2.Page.enable();
    dp2.Page.onFrameNavigated(onFrameNavigated);
    await dp2.Runtime.runIfWaitingForDebugger();
  });

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  testRunner.log('Navigate to localhost');
  await session.navigate('http://localhost:8000/inspector-protocol/resources/security-origin-testpage.html');

  testRunner.log('Navigate to URL without secure scheme');
  await session.navigate('http://devtools.test:8000/inspector-protocol/resources/security-origin-testpage.html');

  testRunner.log('Navigate to URL with secure scheme');
  await session.navigate('https://localhost:8443/inspector-protocol/resources/security-origin-testpage.html');

  testRunner.completeTest();
})

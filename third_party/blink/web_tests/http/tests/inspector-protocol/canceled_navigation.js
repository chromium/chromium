(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  var {page, session, dp} = await testRunner.startBlank(
        `Tests that waitForDebuggerOnStart doesn't crash for canceled navigations.`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  await dp.Page.enable();
  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_USER_AGENT';
    document.body.appendChild(iframe);
  `);

  const sessionId = (await attachedPromise).params.sessionId;
  const dp1 = session.createChild(sessionId).protocol;
  session.evaluate(`document.body.innerHTML = '';`);
  await dp.Page.stopNavigation();
  await dp.Runtime.runIfWaitingForDebugger();

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests frameStartedNavigating events with tab targets and user action opening a new page');

  function testRunnerLog() {
    testRunner.log(
        arguments[0] ?? '', arguments[1] ?? '', testRunner.stabilizeNames,
        ['frameId', 'loaderId', 'sessionId']);
  }

  let onceFrameStartedNavigatingResolve;
  const onceFrameStartedNavigating =
      new Promise(resolve => onceFrameStartedNavigatingResolve = resolve);

  const response = await testRunner.browserP().Target.attachToBrowserTarget();
  const newBrowserSession =
      new TestRunner.Session(testRunner, response.result.sessionId);

  newBrowserSession.protocol.Target.onAttachedToTarget((tabTargetParams) => {
    // Attached to tab target.
    const tabSession =
        newBrowserSession.createChild(tabTargetParams.params.sessionId);
    tabSession.protocol.Target.onAttachedToTarget(async (pageTargetParams) => {
      // Attached to page target.
      const pageSession =
          session.createChild(pageTargetParams.params.sessionId);
      pageSession.protocol.Page.onFrameStartedNavigating(event => {
        testRunnerLog(event, `frameStartedNavigating`);
        onceFrameStartedNavigatingResolve(event);
      });

      await pageSession.protocol.Page.enable();
      void pageSession.protocol.Runtime.runIfWaitingForDebugger();
      void tabSession.protocol.Runtime.runIfWaitingForDebugger();
    });

    void tabSession.protocol.Target.setAutoAttach({
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
    });
  });

  await newBrowserSession.protocol.Target.setAutoAttach({
    autoAttach: true,
    waitForDebuggerOnStart: true,
    flatten: true,
    filter: [{type: 'page', exclude: true}, {}]
  });


  dp.Runtime.evaluate({
    expression: `
        const link = document.createElement('a');
        link.href = '${testRunner.url('../resources/empty.html?link.click')}';
        link.target = '_blank';
        link.textContent = 'Click me!';
        document.body.appendChild(link);
        link.click();`,
    userGesture: true
  });

  await onceFrameStartedNavigating;

  // await new Promise(resolve => setTimeout(resolve, 1000))
  testRunner.completeTest();
})

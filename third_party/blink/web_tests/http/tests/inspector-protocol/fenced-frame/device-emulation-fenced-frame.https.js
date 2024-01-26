(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      '../resources/empty.html',
      'Tests that device emulation affects fenced frames.');

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  // Disable MockScreenOrientation.
  await session.evaluate('testRunner.disableMockScreenOrientation()');

  // Create and navigate inside a fenced frame.
  session.evaluate(function() {
    let ff = document.createElement('fencedframe');
    const url = new URL('../fenced-frame/resources/page-with-title.php',
        location.href);
    ff.config = new FencedFrameConfig(url);
    document.body.appendChild(ff);
  });

  let {sessionId} = (await dp.Target.onceAttachedToTarget()).params;
  let ffSession = session.createChild(sessionId);
  let ffdp = ffSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({enabled: true});
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  // Disable MockScreenOrientation.
  await ffSession.evaluate('testRunner.disableMockScreenOrientation()');

  function dumpMetrics() {
    return {
      'screen.width': screen.width,
      'screen.height': screen.height,
      'screen.orientation.type': screen.orientation.type,
      'screen.orientation.angle': screen.orientation.angle,
    }
  };

  // Test that device metrics override is applied to the nested fenced frame.
  await session.protocol.Emulation.setDeviceMetricsOverride({
    width: 800,
    height: 600,
    deviceScaleFactor: 1.0,
    mobile: true,
    screenOrientation: {type: 'landscapeSecondary', angle: 270},
  });
  testRunner.log(await session.evaluate(dumpMetrics), 'Metrics: ');
  testRunner.log(await ffSession.evaluate(dumpMetrics), 'FF Metrics: ');

  testRunner.completeTest();
})

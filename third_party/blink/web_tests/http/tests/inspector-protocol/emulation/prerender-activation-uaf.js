(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageURL = testRunner.url('../resources/inspector-protocol-page.html');
  const prerenderURL = testRunner.url('../resources/test-page.html');

  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      'Tests that prerender activation does not cause UAF or crash in EmulationHandler.');

  const tp = tabTargetSession.protocol;

  // 1. Enable auto-attach on the Tab target to catch the page session.
  const attachedPromise = tp.Target.onceAttachedToTarget();
  await tp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  // Wait for the page session to be attached.
  const attachedToTarget = await attachedPromise;
  const pageSessionId = attachedToTarget.params.sessionId;
  const pageSession = tabTargetSession.createChild(pageSessionId);
  const pp = pageSession.protocol;

  testRunner.log('Attached to page session via tab session');

  // Navigate to the same origin as the prerender URL.
  await pp.Page.enable();
  await pp.Page.navigate({url: pageURL});
  await pp.Page.onceLoadEventFired();
  testRunner.log('Navigated to base page');

  // 2. Set emulation override on the page session.
  await pp.Emulation.setDeviceMetricsOverride({
    width: 400,
    height: 800,
    deviceScaleFactor: 1,
    mobile: true,
    screenOrientationLockEmulation: true
  });
  testRunner.log('Set device metrics override with screenOrientationLockEmulation: true');

  // 3. Enable Preload domain to track prerendering.
  await pp.Preload.enable();

  // 4. Trigger prerender.
  await pageSession.evaluate(`
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = JSON.stringify({
      prerender: [{
        source: 'list',
        urls: ['${prerenderURL}']
      }]
    });
    document.body.appendChild(script);
  `);
  testRunner.log('Triggered prerender via speculation rules');

  testRunner.log('Waiting for prerender to be ready...');
  while (true) {
    const {params} = await pp.Preload.oncePrerenderStatusUpdated();
    if (params.status === 'Ready') {
      testRunner.log('Prerender is ready');
      break;
    }
    if (params.status === 'Failure') {
      testRunner.log('FAIL: Prerender failed: ' + params.prerenderStatus);
      testRunner.completeTest();
      return;
    }
  }
// 5. Navigate to the prerendered URL to trigger activation.
testRunner.log('Navigating to prerendered URL to trigger activation...');
const detachedPromise = tp.Target.onceDetachedFromTarget(event => event.params.sessionId === pageSessionId);
await pageSession.evaluate(`location.href = '${prerenderURL}'`);

// Wait for the navigation to complete and the old session to be detached.
await detachedPromise;
testRunner.log('Old page session detached');
testRunner.completeTest();
})


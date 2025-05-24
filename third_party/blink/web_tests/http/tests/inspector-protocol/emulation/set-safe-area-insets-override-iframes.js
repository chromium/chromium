(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that the safe-area-(max-)inset-* environment variables can be overridden in OOPIFs.');

  dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const frameAttached = dp.Target.onceAttachedToTarget();
  session.evaluate(() => {
    const frame = document.createElement('iframe');
    frame.src =
        'http://devtools.oopif.test:8000/inspector-protocol/resources/iframe.html';
    document.body.appendChild(frame);
  });

  const sessionId = (await frameAttached).params.sessionId;
  const session2 = session.createChild(sessionId);
  const dp2 = session2.protocol;

  await dp2.Page.enable();
  dp2.Page.onLoadEventFired(async (e) => {
    testRunner.log('iframe loaded, overriding insets');
    await session2.protocol.Emulation.setSafeAreaInsetsOverride({
      insets: {
        top: 1,
        topMax: 2,
        left: 3,
        leftMax: 4,
        bottom: 5,
        bottomMax: 6,
        right: 7,
        rightMax: 8
      }
    });
  });
  const loadEvent = dp2.Page.onceLoadEventFired();
  await session2.protocol.Runtime.runIfWaitingForDebugger();
  await loadEvent;

  testRunner.log('Environment:');
  for (const position of ['top', 'left', 'bottom', 'right']) {
    for (const max of ['', 'max-']) {
      const envVar = `safe-area-${max}inset-${position}`;
      const offset = await session2.evaluate((envVar) => {
        const el = document.createElement('div');
        el.style.position = 'fixed';
        el.style.top = `env(${envVar}, 42)`;
        document.body.appendChild(el);
        const result = el.offsetTop;
        document.body.removeChild(el);
        return result;
      }, envVar);
      testRunner.log(`${envVar}: ${offset}`);
    }
  }

  testRunner.completeTest();
});

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    'Tests that renderer does not crash upon emulation shutdown with media override.');

  dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  // Create first iframe that is designed to survive the second iframe after it's removed
  // in the same process, so we can reliably detect the process crash.
  session.evaluate(`
      function createFrame(query) {
        const frame = document.createElement('iframe');
        frame.src = 'http://devtools.oopif.test:8000/inspector-protocol/resources/iframe.html?' + query;
        document.body.appendChild(frame);
      }
      createFrame('1');
  `);
  const attachedToTarget = (await dp.Target.onceAttachedToTarget()).params;
  const iframeSession = session.createChild(attachedToTarget.sessionId);

  // Create another frame in the same target, then detach it after emulated media is configured.
  session.evaluate(`createFrame('2')`);
  const attachedToTarget2 = (await dp.Target.onceAttachedToTarget()).params;
  const iframeSession2 = session.createChild(attachedToTarget2.sessionId);
  const dp2 = iframeSession2.protocol;
  dp2.Page.enable();
  await dp2.Page.onceLoadEventFired();
  await dp2.Emulation.setEmulatedMedia({
      features: [{name: 'prefers-color-scheme', value: 'dark'}]});
  session.evaluate(`document.body.removeChild(document.body.lastElementChild)`);
  await dp.Target.onceDetachedFromTarget();
  testRunner.log(`Detached from target`);
  // Assure the first frame is alive.
  testRunner.log(`First frame: ${await iframeSession.evaluate('location.href')}`);
  testRunner.completeTest();
});

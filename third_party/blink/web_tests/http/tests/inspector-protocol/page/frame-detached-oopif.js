(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests that frameDetached event is emitted when the OOPIF is gone.');

  const url = 'http://oopif.test:8000/inspector-protocol/resources/empty.html';

  const events = [];
  const logEvent = (event) => {
    events.push(event);
  };
  dp.Page.onFrameAttached(logEvent);
  dp.Page.onFrameDetached(logEvent);
  dp.Target.onTargetDestroyed(logEvent);
  dp.Target.onDetachedFromTarget(logEvent);
  const replacedAttributes = ['parentFrameId', 'frameId', 'sessionId', 'targetId'];

  await dp.Page.enable();
  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await dp.Target.setDiscoverTargets({discover: true});

  testRunner.log('Creating a frame with the URL ' + url);
  const frameLoaded = session.evaluateAsync(`
    window.frame = document.createElement('iframe');
    frame.src = '${url}';
    document.body.appendChild(frame);
    new Promise(resolve => { frame.onload = resolve });
  `);

  const onceFrameAttached = dp.Page.onceFrameAttached();
  const onceFrameDetachedSwap = dp.Page.onceFrameDetached();
  testRunner.log(await onceFrameAttached, 'Attached frame ', replacedAttributes);
  testRunner.log(await onceFrameDetachedSwap, 'Detached frame (swap)', replacedAttributes);
  await frameLoaded;

  testRunner.log('Removing the frame');
  const onceTargetDestroyed = dp.Target.onceTargetDestroyed();
  const onceFrameDetached = dp.Page.onceFrameDetached();
  session.evaluate(() => frame.remove());
  testRunner.log(await onceFrameDetached, 'Detached frame (remove) ', replacedAttributes);
  testRunner.log(await onceTargetDestroyed, 'Destroyed target ', replacedAttributes);

  testRunner.log(events, 'Events (in order)', replacedAttributes);
  testRunner.completeTest();
})

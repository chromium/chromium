(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests Overlay domain commands when navigating an OOPIF subframe to another OOPIF.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});


  testRunner.log('Navigating to first OOPIF...');
  const attachedPromise1 = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    const frame = document.createElement('iframe');
    frame.id = 'oopif';
    frame.src = 'https://devtools.oopif-a.test:8443/inspector-protocol/resources/iframe.html';
    document.body.appendChild(frame);
  `);

  const attached1 = await attachedPromise1;
  testRunner.log('Attached to subframe.');
  const childSession = session.createChild(attached1.params.sessionId);
  const childDp = childSession.protocol;

  await childDp.DOM.enable();
  await childDp.Overlay.enable();
  await childDp.Page.enable();

  const root1 = (await childDp.DOM.getDocument()).result.root;
  const highlightResult1 = await childDp.Overlay.highlightNode({
    highlightConfig: {contentColor: {r: 255, g: 0, b: 0, a: 0.5}},
    nodeId: root1.nodeId,
  });
  testRunner.log(highlightResult1, 'First highlight result:');

  testRunner.log('Navigating to second OOPIF...');
  session.evaluate(`
    document.getElementById('oopif').src = 'https://devtools.oopif-b.test:8443/inspector-protocol/resources/iframe.html';
  `);

  await childDp.Page.onceDomContentEventFired();
  testRunner.log('Navigated to second OOPIF.');

  const root2 = (await childDp.DOM.getDocument()).result.root;
  const highlightResult2 = await childDp.Overlay.highlightNode({
    highlightConfig: {contentColor: {r: 0, g: 255, b: 0, a: 0.5}},
    nodeId: root2.nodeId,
  });
  testRunner.log(highlightResult2, 'Second highlight result:');

  testRunner.completeTest();
})

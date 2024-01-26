(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Tests DOM.getFrameOwner method.');
  await dp.Target.setDiscoverTargets({discover: true});
  var [r] = await Promise.all([
    dp.Target.onceTargetCreated(),
    session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.id = 'outer_frame';
    iframe.src = 'data:text/html,<iframe id=inner_frame src="http://devtools.oopif.test:8000/resources/dummy.html">';
    document.body.appendChild(iframe);
  `),
  ]);
  await dp.DOM.enable();
  r = await dp.DOM.getFrameOwner({frameId: r.params.targetInfo.targetId});
  r = await dp.DOM.describeNode({backendNodeId: r.result.backendNodeId});
  testRunner.log(r.result.node.nodeName + ' ' + r.result.node.attributes);
  testRunner.completeTest();
})

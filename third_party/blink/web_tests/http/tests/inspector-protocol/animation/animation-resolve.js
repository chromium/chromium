(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
  `, 'Tests how animation could be resolved into remote object.');

  dp.Animation.enable();
  session.evaluate(`
    window.player = node.animate([{ width: '100px' }, { width: '200px' }], 2000);
  `);

  var result = await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  var response = await dp.Animation.resolveAnimation({ animationId: result.params.animation.id });
  testRunner.log('Remote object:');
  testRunner.log(response.result.remoteObject.className);
  testRunner.completeTest();
})

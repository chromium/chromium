(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; width: 100px'></div>
  `, 'Tests that the animation is correctly paused.');

  await dp.Animation.enable();

  const animationStartedPromise = dp.Animation.onceAnimationStarted();
  await session.evaluate(`
    window.animation = node.animate([{ width: '100px' }, { width: '2000px' }], { duration: 0, fill: 'forwards' });
  `);

  var id = (await animationStartedPromise).params.animation.id;
  testRunner.log('Animation started');
  var width = await session.evaluate('node.offsetWidth');
  testRunner.log('Box is animating: ' + (width != 100).toString());
  await dp.Animation.setPaused({ animations: [ id ], paused: true });
  await session.evaluate('animation.cancel()');
  width = await session.evaluate('node.offsetWidth');
  testRunner.log('Animation paused');
  testRunner.log('Box is animating: ' + (width != 100).toString());
  await dp.Animation.releaseAnimations({ animations: [ id ] });
  width = await session.evaluate('node.offsetWidth');
  testRunner.log('Animation released');
  testRunner.log('Box is animating: ' + (width != 100).toString());
  testRunner.completeTest();
})

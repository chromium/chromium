(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
  `, 'Tests that animation with null effect/target works.');

  dp.Animation.enable();
  session.evaluate(`
    var effect = new KeyframeEffect(null, null, {duration: 1000});
    var animation = node.animate({});
    animation.effect = effect;
  `);
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');

  session.evaluate(`
    var animation = node.animate(null);
    animation.effect = null;
  `);
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');

  testRunner.completeTest();
})

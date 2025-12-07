(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
    <div id='node2' style='background-color: blue; height: 100px'></div>
  `, 'Tests that animation iterations are reported correctly.');

  dp.Animation.enable();

  // Test for infinite iterations
  let animationStartedPromise = dp.Animation.onceAnimationStarted();
  session.evaluate(`
    window.animation = node.animate([{ width: '100px' }, { width: '200px' }], { duration: 1000, iterations: Infinity });
  `);
  let response = await animationStartedPromise;
  let animation = response.params.animation;
  testRunner.log('Animation with infinite iterations started');
  testRunner.log(`iterations: ${'iterations' in animation.source ? animation.source.iterations : 'omitted'}`);

  // Test for finite iterations
  animationStartedPromise = dp.Animation.onceAnimationStarted();
  session.evaluate(`
    window.animation2 = node2.animate([{ width: '100px' }, { width: '200px' }], { duration: 1000, iterations: 5 });
  `);
  response = await animationStartedPromise;
  animation = response.params.animation;
  testRunner.log('Animation with finite iterations started');
  testRunner.log(`iterations: ${animation.source.iterations}`);


  testRunner.completeTest();
})

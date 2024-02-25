(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; width: 100px'></div>
  `, 'Tests animationCanceled notification.');

  dp.Animation.onAnimationCreated(() => testRunner.log('Animation created'));
  dp.Animation.onAnimationStarted(() => testRunner.log('Animation started'));
  dp.Animation.onAnimationCanceled(() => {
    testRunner.log('Animation canceled')
    testRunner.completeTest();
  });
  dp.Animation.enable();

  session.evaluate(`
    node.style.transition = '1s';
    node.offsetTop;
    node.style.width = '200px';
    node.offsetTop;
    // Deliberately delay for two RAFs, which causes the animation to start
    // before we cancel it by clearing the transition.
    window.requestAnimationFrame(function() {
      window.requestAnimationFrame(function() {
        node.style.transition = 'none';
      });
    });
  `);
})

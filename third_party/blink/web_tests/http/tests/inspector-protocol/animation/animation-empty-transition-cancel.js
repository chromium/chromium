(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; width: 100px'></div>
  `, 'Test that canceling css transition is reported over protocol.');

  dp.Animation.onAnimationCreated(() => testRunner.log('Animation created'));
  dp.Animation.onAnimationCanceled(() => testRunner.log('Animation canceled'));
  dp.Animation.enable();
  await session.evaluate(`
    node.offsetTop;
    node.style.transition = "1s";
    node.offsetTop;
    node.style.width = "200px";
    node.offsetTop;
    node.style.transition = "none";
    node.offsetTop;
  `);
  testRunner.completeTest();
})

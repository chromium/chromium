(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
  `, 'Tests that animation creation and start are reported over protocol.');

  dp.Animation.enable();
  session.evaluate(`node.animate([{ width: '100px' }, { width: '200px' }], 2000);`);
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  testRunner.completeTest();
})

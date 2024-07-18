(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
  `, 'Tests that after calling `enable` animation domain emits animation created & started events for already running animations.');

  session.evaluate(`node.animate([{ width: '100px' }, { width: '200px' }], { duration: 1000, iterations: Infinity });`);
  const onceAnimationCreatedPromise = dp.Animation.onceAnimationCreated();
  const onceAnimationStartedPromise = dp.Animation.onceAnimationStarted();
  await dp.Animation.enable();
  await onceAnimationCreatedPromise;
  await onceAnimationStartedPromise;
  testRunner.log('Animation created & started is emitted for an already running animation');
  testRunner.completeTest();
})

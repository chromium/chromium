(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px; transition: width 100ms; width: 100px'></div>
  `, 'Tests replaying css animation.');

  dp.Animation.enable();
  session.evaluate(`node.style.width = '200px';`);

  var response = await dp.Animation.onceAnimationStarted();
  testRunner.log("Animation started");

  for (var run = 0; run < 5; run++) {
    await dp.Animation.seekAnimations({ animations: [ response.params.animation.id ], currentTime: 0 });
    testRunner.log("Animation seeked");
  }
  testRunner.completeTest();
})

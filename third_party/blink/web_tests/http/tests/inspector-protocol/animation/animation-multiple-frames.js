(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests animation notifications from multiple frames.');

  await session.evaluate(`
    function appendIframe() {
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/test-page-trigger-animation.html')}';
      document.body.appendChild(frame);
    }
  `);

  async function appendFrame() {
    await session.evaluate(`appendIframe()`);
    testRunner.log('Frame appended');
  }

  var numberAnimationsCaptured = 0;
  var lastStartTime = undefined;

  dp.Animation.onAnimationStarted(data => {
    var animation = data.params.animation;

    if (!lastStartTime || animation.startTime >= lastStartTime)
      testRunner.log('Animation started: start time is valid');
    else if (lastStartTime)
      testRunner.log(`Animation started: invalid startTime: ${animation.startTime} < ${lastStartTime}`);
    lastStartTime = animation.startTime;
    numberAnimationsCaptured++;

    if (numberAnimationsCaptured < 10)
      appendFrame();
    else
      testRunner.completeTest();
  });

  dp.Animation.enable();
  appendFrame();
})

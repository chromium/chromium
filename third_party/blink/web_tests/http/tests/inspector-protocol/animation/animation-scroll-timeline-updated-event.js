(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startHTML(`
    <style>
      @keyframes --appear {
        from {
          translate: transformX(0);
        }

        to {
          translate: transformX(150px);
        }
      }

      #node {
        animation: --appear;
        animation-range-start: 10px;
        animation-timeline: scroll();
        animation-play-state: paused;
      }
    </style>
    <div id='container' style='height: 50px; overflow: auto;'>
      outside text
      <div id='node' style='background-color: red; height: 100px'>inside text</div>
    </div>
  `, 'Tests that `animationUpdated` event is emitted when the animation is updated');

  await dp.Animation.enable();
  session.evaluate(`
    node.style.animationPlayState = "running";
  `);

  const resp = await dp.Animation.onceAnimationStarted();
  const animation = resp.params.animation;
  assertTrue(Math.floor(animation.startTime) === 14, "startTime is reported correctly as percentage.");

  const animationUpdatedEvent = dp.Animation.onceAnimationUpdated();
  session.evaluate(`
      const animation = node.getAnimations()[0];
      animation.startTime = CSS.percent(50);
  `);
  const animationAfterUpdate = (await animationUpdatedEvent).params.animation;
  assertTrue(Math.floor(animationAfterUpdate.startTime) === 50, "startTime is reported correctly as percentage in the animation coming from animationUpdated event.");

  testRunner.completeTest();
  function assertTrue(expression, message) {
    if (expression) {
      testRunner.log("PASS: " + message);
    } else {
      testRunner.log("FAIL: " + message);
      testRunner.completeTest();
    }
  }
})
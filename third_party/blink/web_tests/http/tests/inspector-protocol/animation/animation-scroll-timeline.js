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
  `, 'Tests that the viewOrScrollTimeline is included for scroll driven animations');

  await dp.Animation.enable();
  await session.evaluate(`
    node.style.animationPlayState = "running";
  `);

  const resp = await dp.Animation.onceAnimationStarted();
  const animation = resp.params.animation;

  assertTrue(Math.floor(animation.startTime) === 14, "startTime is reported correctly as percentage.");
  assertTrue(Math.floor(animation.source.duration) === 85, "source.duration is reported correctly as percentage.");
  assertTrue(animation.viewOrScrollTimeline.axis === "vertical", "Axis of view or scroll timeline is correct.");
  assertTrue(animation.viewOrScrollTimeline.startOffset !== undefined, "startOffset exists.");
  assertTrue(animation.viewOrScrollTimeline.endOffset !== undefined, "endOffset exists.");
  assertTrue(animation.viewOrScrollTimeline.sourceNodeId !== undefined, "sourceNodeId exists.");

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
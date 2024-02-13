(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startHTML(`
    <div id='container' style='height: 500px; overflow: auto;'>
      <div id='node' style='background-color: red; height: 1000px'></div>
    </div>
  `, 'Tests that the viewOrScrollTimeline is included for scroll driven animations');

  await dp.Animation.enable();
  await session.evaluate(`
    const tl = new ViewTimeline({
      subject: document.getElementById('node'),
    });
    window.animation = node.animate([{ width: '100px' }, { width: '200px' }], { timeline: tl, rangeStart: 'entry 25%', rangeEnd: 'cover 50%' });
  `);

  const resp = await dp.Animation.onceAnimationStarted();
  const animation = resp.params.animation;

  assertTrue(Math.floor(animation.startTime) === 8, "startTime is reported correctly as percentage.");
  assertTrue(Math.floor(animation.source.duration) === 41, "duration is reported correctly as percentage.");
  assertTrue(animation.viewOrScrollTimeline.axis === "vertical", "Axis of view or scroll timeline is correct.");
  assertTrue(animation.viewOrScrollTimeline.startOffset !== undefined, "startOffset exists.");
  assertTrue(animation.viewOrScrollTimeline.endOffset !== undefined, "endOffset exists.");
  assertTrue(animation.viewOrScrollTimeline.sourceNodeId !== undefined, "sourceNodeId exists.");
  assertTrue(animation.viewOrScrollTimeline.subjectNodeId !== undefined, "subjectNodeId exists.");

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
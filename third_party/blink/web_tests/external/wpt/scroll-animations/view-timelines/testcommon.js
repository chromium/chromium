'use strict';

function CreateViewTimelineOpacityAnimation(test, target,
                                            orientation = 'block') {
  const anim =
      target.animate(
          { opacity: [0.3, 0.7] },
          {
            timeline: new ViewTimeline({
              subject: target,
              axis: orientation
            }),
            fill: 'none'
          });
  test.add_cleanup(() => {
    anim.cancel();
  });
  return anim;
}

// Sets the start and end delays for a view timeline and ensures that the
// range aligns with expected values.
//
// Sample call:
// await runTimelineDelayTest(t, {
//   delay: { phase: 'cover', percent: CSS.percent(0) } ,
//   endDelay: { phase: 'cover', percent: CSS.percent(100) },
//   rangeStart: 600,
//   rangeEnd: 900
// });
async function runTimelineDelayTest(t, options) {
  container.scrollLeft = 0;
  await waitForNextFrame();

  const anim = CreateViewTimelineOpacityAnimation(t, target, 'inline');
  anim.effect.updateTiming({
    delay: options.delay,
    endDelay: options.endDelay,
    // Set fill to accommodate floating point precision errors at the
    // endpoints.
    fill: 'both'
  });
  const timeline = anim.timeline;
  await anim.ready;

  const delayToString = delay => {
    const parts = [];
    if (delay.phase)
      parts.push(delay.phase);
    if (delay.percent)
      parts.push(`${delay.percent.value}%`);
    return parts.join(' ');
  };

  // Advance to the start offset, which triggers entry to the active phase.
  container.scrollLeft = options.rangeStart;
  await waitForNextFrame();
  const range =
     `${delayToString(options.delay)} to ` +
     `${delayToString(options.endDelay)}`;
  assert_equals(getComputedStyle(target).opacity, '0.3',
                `Effect at the start of the active phase: ${range}`);

  // Advance to the midpoint of the animation.
  container.scrollLeft = (options.rangeStart + options.rangeEnd) / 2;
  await waitForNextFrame();
  assert_equals(getComputedStyle(target).opacity,'0.5',
                `Effect at the midpoint of the active range: ${range}`);

  // Advance to the end of the animation.
  container.scrollLeft = options.rangeEnd;
  await waitForNextFrame();
  assert_equals(getComputedStyle(target).opacity, '0.7',
                `Effect is in the active phase at effect end time: ${range}`);
}


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
    // anim.cancel();
  });
  return anim;
}

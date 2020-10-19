const delta_for_scroll = 100;
// See synthetic_gesture_params.h.
const ScrollSource = { Touch: 1, Wheel : 2};

function ensurePlatformAPIExists() {
  if (!window.chrome || !window.chrome.gpuBenchmarking)
    throw "'gpuBenchmarking' needed for this test.";
}

function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // For now, we just rAF twice. It would be nice to have a proper mechanism
    // for this.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

function smoothScrollBy(scrollOffset, xPosition, yPosition, direction, source, speed, preciseScrollingDeltas) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var scrollOffsetX = 0;
      var scrollOffsetY = 0;
      if (direction == "down") {
        scrollOffsetY = scrollOffset;
      } else if (direction == "up") {
        scrollOffsetY = -scrollOffset;
      } else if (direction == "right") {
        scrollOffsetX = scrollOffset;
      } else if (direction == "left") {
        scrollOffsetX = -scrollOffset;
      } else if (direction == "upleft") {
        scrollOffsetX = -scrollOffset;
        scrollOffsetY = -scrollOffset;
      } else if (direction == "upright") {
        scrollOffsetX = scrollOffset;
        scrollOffsetY = -scrollOffset;
      } else if (direction == "downleft") {
        scrollOffsetX = -scrollOffset;
        scrollOffsetY = scrollOffset;
      } else if (direction == "downright") {
        scrollOffsetX = scrollOffset;
        scrollOffsetY = scrollOffset;
      }
      chrome.gpuBenchmarking.smoothScrollByXY(scrollOffsetX, scrollOffsetY, resolve, xPosition,
        yPosition, source, speed, preciseScrollingDeltas);
    } else {
      reject();
    }
  });
}

async function touchScroll(direction, start_x, start_y) {
  ensurePlatformAPIExists("touch");
  await waitForCompositorCommit();
  await smoothScrollBy(delta_for_scroll, start_x, start_y, direction, ScrollSource.Touch);
  await waitForCompositorCommit();
}

function pinchZoomGesture(
  touch_point_1, touch_point_2, move_offset, offset_upper_bound) {
  return new Promise((resolve) => {
    var pointerActions = [{'source': 'touch'}, {'source': 'touch'}];
    var pointerAction1 = pointerActions[0];
    var pointerAction2 = pointerActions[1];
    pointerAction1.actions = [];
    pointerAction2.actions = [];
    pointerAction1.actions.push(
        {name: 'pointerDown', x: touch_point_1.x, y: touch_point_1.y});
    pointerAction2.actions.push(
        {name: 'pointerDown', x: touch_point_2.x, y: touch_point_2.y});
    for (var offset = move_offset; offset < offset_upper_bound; offset += move_offset) {
      pointerAction1.actions.push({
          name: 'pointerMove',
          x: (touch_point_1.x - offset),
          y: touch_point_1.y,
      });
      pointerAction2.actions.push({
          name: 'pointerMove',
          x: (touch_point_2.x + offset),
          y: touch_point_2.y,
      });
    }
    pointerAction1.actions.push({name: 'pointerUp'});
    pointerAction2.actions.push({name: 'pointerUp'});
    chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
  })
}

async function pinchZoom(direction, start_x_1, start_y_1, start_x_2, start_y_2) {
  ensurePlatformAPIExists("touch");
  let zoom_in = direction === "in";
  let delta = zoom_in ? -delta_for_scroll : delta_for_scroll;
  let move_offset = 10;
  let offset_upper_bound = 80;
  await waitForCompositorCommit();
  await pinchZoomGesture(
    {x: start_x_1, y: start_y_1},
    {x: start_x_2, y: start_y_2},
    move_offset,
    offset_upper_bound);
  await waitForCompositorCommit();
}

async function wheelScroll(direction, start_x, start_y) {
  ensurePlatformAPIExists("wheel");
  await waitForCompositorCommit();
  await smoothScrollBy(delta_for_scroll, start_x, start_y, direction, ScrollSource.Wheel);
  await waitForCompositorCommit();
}

window.input_api_ready = true;
if (window.resolve_on_input_api_ready)
  window.resolve_on_input_api_ready();

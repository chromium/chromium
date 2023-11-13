// Helper functions for snapchanged-on-user-* tests.

// This performs a touch scroll on |scroller| using the coordinates provided
// in |start_pos| and |end_pos|.
// It is meant for use in snapchanged tests for triggering snapchanged events
// when touch scrolling from |start_pos| to |end_pos|.
function snapchanged_touch_scroll_helper(start_pos, end_pos) {
  return new test_driver.Actions()
    .addPointer("TestPointer", "touch")
    .pointerMove(start_pos.x, start_pos.y)
    .pointerDown()
    .addTick()
    .pause(200)
    .pointerMove(end_pos.x, end_pos.y)
    .addTick()
    .pointerUp()
    .send();
}

// This drags the provided |scroller|'s scrollbar  vertically by |drag_amt|.
// Snapchanged tests should provide a |drag_amt| that would result in a
// snapchanged event being triggered.
const vertical_offset_into_scrollbar = 30;
function snapchanged_scrollbar_drag_helper(scroller, scrollbar_width, drag_amt) {
  let x, y, bounds;
  if (scroller == document.scrollingElement) {
    bounds = document.documentElement.getBoundingClientRect();
    x = window.innerWidth - Math.round(scrollbar_width / 2);
  } else {
    bounds = scroller.getBoundingClientRect();
    x = bounds.right - Math.round(scrollbar_width / 2);
  }
  y = bounds.top + vertical_offset_into_scrollbar;
  return new test_driver.Actions()
    .addPointer('TestPointer', 'mouse')
    .pointerMove(x, y)
    .pointerDown()
    .pointerMove(x, y + drag_amt)
    .addTick()
    .pointerUp()
    .send();
}

// This tests that snapchanged doesn't fire for a user (wheel) scroll that
// snaps back to the same element. snapchanged tests should provide a |delta|
// small enough that no change in |scroller|'s snap targets occurs at the end of
// the scroll.
async function test_no_snapchanged(test, scroller, delta) {
  const listening_element = scroller == document.scrollingElement
      ? document : scroller;
  checkSnapchangedSupport(test);
  await waitForScrollReset(test, scroller);
  await waitForCompositorCommit();
  let snapchanged_promise = waitForSnapChangedEvent(listening_element);
  // Set the scroll destination to just a little off (0, 0) top so we snap
  // back to the top box.
  await new test_driver.Actions().scroll(0, 0, delta, delta).send();
  let evt = await snapchanged_promise;
  assert_equals(evt, null, "no snapchanged since scroller is back to top");
  assert_equals(scroller.scrollTop, 0, "scroller snaps back to the top");
  assert_equals(scroller.scrollLeft, 0, "scroller snaps back to the left");
}

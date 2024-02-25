async function testScrollThumbNonScrolls(params) {
  await waitForCompositorCommit();
  resetScrollOffset(params.scroller);

  const scrollRect = scroller.getBoundingClientRect();

  // Direction: rtl changes the x-wise position of the vertical scrollbar
  const rtl = params.scroller.style.direction === "rtl";

  // Testing the vertical scrollbar thumb.
  const x = rtl ? scrollRect.left + params.TRACK_WIDTH / 2 : scrollRect.right - params.TRACK_WIDTH / 2;
  const y = scrollRect.top + params.BUTTON_WIDTH + 2;

  await mouseMoveTo(x, y);
  await mouseDownAt(x, y);
  assert_equals(params.scroller.scrollTop, 0, "Mousedown on vertical scrollbar thumb is not expected to scroll.");

  await mouseMoveTo(x, y - 10, Buttons.LEFT);
  assert_equals(params.scroller.scrollTop, 0, "Vertical thumb drag beyond the track should not cause a scroll.");

  await mouseMoveTo(x, y, Buttons.LEFT);
  assert_equals(params.scroller.scrollTop, 0, "Vertical thumb drag beyond the track and back should not cause a scroll.");

  await mouseUpAt(x, y);
}

async function testThumbScrolls(params) {
  await waitForCompositorCommit();
  resetScrollOffset(params.scroller);

  const scrollRect = scroller.getBoundingClientRect();

  // Direction: rtl changes the x-wise position of the vertical scrollbar
  const rtl = params.scroller.style.direction === "rtl";

  // Testing the vertical scrollbar thumb. Move the pointer to the edge of
  // the scrollbar border to verify that capturing and dragging work across
  // the whole width of the scrollbar track.
  let x = rtl ? scrollRect.left + 2 : scrollRect.right - 2;
  let y = scrollRect.top + params.BUTTON_WIDTH + 2;
  let asc_increments = [15, 10, 7, 6, 2];
  let asc_offsets = { linux: [549, 915, 915, 915, 915], win: [361, 601, 770, 915, 915], mac: [211, 351, 450, 534, 563] }[params.platform];
  let desc_increments = [3, 2, 5, 9, 21];
  let desc_offsets = { linux: [915, 915, 915, 768, 0], win: [890, 842, 722, 505, 0], mac: [520, 492, 422, 295, 0] }[params.platform];
  // Fluent scrollbars have different minimum length thumbs which changes how
  // far dragging the thumb scrolls the content.
  let asc_offsets_fluent = {
    linux: [361, 601, 770, 915, 915],
    win: [361, 601, 770, 915, 915],
    mac: [211, 351, 450, 534, 563]
  }[params.platform];
  let desc_offsets_fluent = {
    linux: [890, 842, 722, 505, 0],
    win: [890, 842, 722, 505, 0],
    mac: [520, 492, 422, 295, 0]
  }[params.platform];

  await mouseMoveTo(x, y);
  await mouseDownAt(x, y);

  // Scroll down
  for (var i = 0; i < 5; i++) {
    y += asc_increments[i];
    await mouseMoveTo(x, y, Buttons.LEFT);
    // TODO(crbug.com/1009892): Sometimes there is 1px difference in threaded scrollbar scrolling mode.
    // Change assert_approx_equals(..., 1, ...) to assert_equals(...) when the bug is fixed.
    let expected_offset = internals.runtimeFlags.fluentScrollbarsEnabled ?
        asc_offsets_fluent[i] :
        asc_offsets[i];
    assert_approx_equals(
        params.scroller.scrollTop, expected_offset, 1,
        'Vertical thumb drag downwards did not scroll as expected at ' +
            asc_increments[i] + ' - ');
  };

  // Scroll up
  for (var i = 0; i < 5; i++) {
    y -= desc_increments[i];
    await mouseMoveTo(x, y, Buttons.LEFT);
    // TODO(crbug.com/1009892): Ditto.
    let expected_offset = internals.runtimeFlags.fluentScrollbarsEnabled ?
        desc_offsets_fluent[i] :
        desc_offsets[i];
    assert_approx_equals(
        params.scroller.scrollTop, expected_offset, 1,
        'Vertical thumb drag upwards did not scroll as expected at ' +
            desc_increments[i] + ' - ');
  };

  await mouseUpAt(x, y);
  assert_equals(params.scroller.scrollTop, 0, "Mouseup on vertical scrollbar thumb is not expected to scroll.");

  // Since the horizontal scrolling is essentially the same codepath as vertical,
  // this need not be tested in the interest of making the test run faster.
}

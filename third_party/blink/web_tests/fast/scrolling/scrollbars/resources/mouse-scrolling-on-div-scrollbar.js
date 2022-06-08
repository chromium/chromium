async function testArrows(params) {
  // Scrollbars on Mac don't have arrows. This test is irrelevant.
  if (navigator.userAgent.includes("Mac OS X"))
    return;

  await waitForCompositorCommit();
  resetScrollOffset(params.scroller);

  const scrollRect = scroller.getBoundingClientRect();

  // For testing on RTL divs, two things are different. The vertical scrollbar is on the opposite side,
  // including the scroll corner. Horizontal scrolling on RTL starts at 0 for the rightmost position
  // and counts downwards into the negatives.
  const rtl = params.scroller.style.direction === "rtl";

  // Click on the Down arrow
  let x = rtl ? scrollRect.left + params.BUTTON_WIDTH / 2 : scrollRect.right - params.BUTTON_WIDTH / 2;
  let y = scrollRect.bottom - params.SCROLL_CORNER - params.BUTTON_WIDTH / 2;
  await mouseClickOn(x, y);
  await waitForAnimationEndTimeBased(() => { return params.scroller.scrollTop; });
  assert_equals(params.scroller.scrollTop, params.SCROLL_AMOUNT, "Pressing the down arrow didn't scroll.");

  // Click on the Up arrow
  x = rtl ? scrollRect.left + params.BUTTON_WIDTH / 2 : scrollRect.right - params.BUTTON_WIDTH / 2;
  y = scrollRect.top + params.BUTTON_WIDTH / 2;
  await mouseClickOn(x, y);
  await waitForAnimationEndTimeBased(() => { return params.scroller.scrollTop; });
  assert_equals(params.scroller.scrollTop, 0, "Pressing the up arrow didn't scroll.");

  async function scrollRight() {
    // Click on the Right arrow
    x = rtl ? scrollRect.right - params.BUTTON_WIDTH / 2 : scrollRect.right - params.SCROLL_CORNER - params.BUTTON_WIDTH / 2;
    y = scrollRect.bottom - params.BUTTON_WIDTH / 2;
    await mouseClickOn(x, y);
    await waitForAnimationEndTimeBased(() => { return params.scroller.scrollLeft; });
    assert_equals(params.scroller.scrollLeft, rtl ? 0 : params.SCROLL_AMOUNT, "Pressing the right arrow didn't scroll.");
  }

  async function scrollLeft() {
    // Click on the Left arrow
    x = rtl ? scrollRect.left + params.SCROLL_CORNER + params.BUTTON_WIDTH / 2 : scrollRect.left + params.BUTTON_WIDTH / 2;
    y = scrollRect.bottom - params.BUTTON_WIDTH / 2;
    await mouseClickOn(x, y);
    await waitForAnimationEndTimeBased(() => { return params.scroller.scrollLeft; });
    assert_equals(params.scroller.scrollLeft, rtl ? -params.SCROLL_AMOUNT : 0, "Pressing the left arrow didn't scroll.");
  }

  //For RTL, horizontal scrollbar starts on the rightmost position, so we need to scroll left first;
  if (rtl) {
    await scrollLeft();
    await scrollRight();
  } else {
    await scrollRight();
    await scrollLeft();
  }
}

async function testTrackparts(params) {
  await waitForCompositorCommit();
  resetScrollOffset(params.scroller);

  const scrollRect = scroller.getBoundingClientRect();

  // For testing on RTL divs, two things are different. The vertical scrollbar is on the opposite side,
  // including the scroll corner. Horizontal scrolling on RTL starts at 0 for the rightmost position
  // and counts downwards into the negatives.
  const rtl = params.scroller.style.direction === "rtl";

  // Click on the track part just above the down arrow.
  assert_equals(params.scroller.scrollTop, 0, "Div is not at 0 offset.");
  let x = rtl ? scrollRect.left + params.BUTTON_WIDTH / 2 : scrollRect.right - params.BUTTON_WIDTH / 2;
  let y = scrollRect.bottom - params.SCROLL_CORNER - params.BUTTON_WIDTH - 2;
  await mouseClickOn(x, y);
  await waitForAnimationEndTimeBased(() => { return params.scroller.scrollTop; });
  assert_approx_equals(params.scroller.scrollTop, 74, 1, "Pressing the down trackpart didn't scroll.");

  // Click on the track part just below the up arrow.
  x = rtl ? scrollRect.left + params.BUTTON_WIDTH / 2 : scrollRect.right - params.BUTTON_WIDTH / 2;
  y = scrollRect.top + params.BUTTON_WIDTH + 2;
  await mouseClickOn(x, y);
  await waitForAnimationEndTimeBased(() => { return params.scroller.scrollTop; });
  assert_equals(params.scroller.scrollTop, 0, "Pressing the up trackpart didn't scroll.");

  async function scrollRight() {
    // Click on the track part just to the left of the right arrow.
    x = rtl ? scrollRect.right - params.BUTTON_WIDTH - 2 : scrollRect.right - params.SCROLL_CORNER - params.BUTTON_WIDTH - 2;
    y = scrollRect.bottom - params.BUTTON_WIDTH / 2;
    await mouseClickOn(x, y);
    await waitForAnimationEndTimeBased(() => { return params.scroller.scrollLeft; });
    assert_approx_equals(params.scroller.scrollLeft, rtl ? 0 : 74, 1, "Pressing the right trackpart didn't scroll.");
  }

  async function scrollLeft() {
    // Click on the track part just to the right of the left arrow.
    x = rtl ? scrollRect.left + params.SCROLL_CORNER + params.BUTTON_WIDTH + 2 : scrollRect.left + params.BUTTON_WIDTH + 2;
    y = scrollRect.bottom - params.BUTTON_WIDTH / 2;
    await mouseClickOn(x, y);
    await waitForAnimationEndTimeBased(() => { return params.scroller.scrollLeft; });
    assert_approx_equals(params.scroller.scrollLeft, rtl ? -74 : 0, 1, "Pressing the left trackpart didn't scroll.");
  }

  if (rtl) {
    await scrollLeft();
    await scrollRight();
  } else {
    await scrollRight();
    await scrollLeft();
  }
}

async function testArrows(scroller) {
  // Scrollbars on Mac don't have arrows. This test is irrelevant.
  if (navigator.userAgent.includes("Mac OS X"))
    return;

  await waitForCompositorCommit();
  await waitForScrollReset(scroller);

  // For testing on RTL divs, two things are different. The vertical scrollbar is on the opposite side,
  // including the scroll corner. Horizontal scrolling on RTL starts at 0 for the rightmost position
  // and counts downwards into the negatives.
  const rtl = scroller.style.direction === "rtl";

  const expectedScroll =
      getScrollbarButtonScrollDelta(scroller).y;

  // Click on the Down arrow
  let position = downArrow(scroller);
  await clickScroll(position.x, position.y, scroller);
  assert_equals(scroller.scrollTop, expectedScroll,
                "Pressing the down arrow didn't scroll.");

  // Click on the Up arrow
  position = upArrow(scroller);
  await clickScroll(position.x, position.y, scroller);
  assert_equals(scroller.scrollTop, 0,
                "Pressing the up arrow didn't scroll.");

  async function scrollRight() {
    const position = rightArrow(scroller);
    await clickScroll(position.x, position.y, scroller);
    assert_equals(scroller.scrollLeft, rtl ? 0 : expectedScroll,
                  "Pressing the right arrow didn't scroll.");
  }

  async function scrollLeft() {
    const position = leftArrow(scroller);
    await clickScroll(position.x, position.y, scroller);
    assert_equals(scroller.scrollLeft, rtl ? -expectedScroll : 0,
                  "Pressing the left arrow didn't scroll.");
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

async function testTrackparts(scroller) {
  await waitForCompositorCommit();
  await waitForScrollReset(scroller);

  // For testing on RTL divs, two things are different. The vertical scrollbar is on the opposite side,
  // including the scroll corner. Horizontal scrolling on RTL starts at 0 for the rightmost position
  // and counts downwards into the negatives.
  const rtl = scroller.style.direction === "rtl";

  // Click on the bottom of the track part (just above the down arrow if any).
  let position = trackBottom(scroller);
  await clickScroll(position.x, position.y, scroller);
  assert_approx_equals(scroller.scrollTop, 74, 1,
                       "Pressing the down trackpart didn't scroll.");

  // Click on the top of the track part (just below the up arrow if any).
  position = trackTop(scroller);
  await clickScroll(position.x, position.y, scroller);
  assert_equals(scroller.scrollTop, 0,
                "Pressing the up trackpart didn't scroll.");

  async function scrollRight() {
    // Click on the right-side of track part (just to the left of the right
    // arrow if any).
    const position = trackRight(scroller);
    await clickScroll(position.x, position.y, scroller);
    assert_approx_equals(scroller.scrollLeft, rtl ? 0 : 74, 1,
                         "Pressing the right trackpart didn't scroll.");
  }

  async function scrollLeft() {
    // Click on the left-side of the track part (just to the right of the left
    // arrow if any).
    const position = trackLeft(scroller);
    await clickScroll(position.x, position.y, scroller);
    assert_approx_equals(scroller.scrollLeft, rtl ? -74 : 0, 1,
                         "Pressing the left trackpart didn't scroll.");
  }

  if (rtl) {
    await scrollLeft();
    await scrollRight();
  } else {
    await scrollRight();
    await scrollLeft();
  }
}

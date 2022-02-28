function validateHoverState(elementList, hoverIndex, reject, message) {
  for (let i = 0; i < elementList.length; i++) {
    if (elementList[i].matches(':hover') != (i == hoverIndex))
      reject(message);
  }
}

function runHoverStateOnScrollTest(scrollCallback, targetIndex) {
  const runTest = async (resolve, reject) => {
    await waitForCompositorCommit();

    let x = array[0].offsetLeft + array[0].clientWidth / 2;
    let y = array[0].offsetTop + array[0].clientHeight / 2;
    // Move cursor to 1st element.
    await mouseMoveTo(x, y);
    await waitFor( () => {
      return array[0].matches(":hover");
    }, 'wait for move to 1st element');

    assert_equals(document.scrollingElement.scrollTop, 0);

    validateHoverState(array, 0, reject,
                       'Not hovering over the first element');

    // While scrolling the hover state shouldn't change. Note, this
    // behavior is not addressed in the specs. The rationale is to prevent the
    // scroll from potentially affecting layout.
    const hoverStateCheck =  () => {
      validateHoverState(array, 0, reject,
                         'Unexpected change to hover state during scroll');
    };
    const scrollListener =
        document.addEventListener('scroll', hoverStateCheck);

    const scrollEndPromise = waitForScrollendEvent(document);
    scrollCallback();
    await scrollEndPromise;

    document.removeEventListener('scroll', scrollListener);

    await waitForCompositorCommit();

    // Once scrolling is complete, the hover state must update.
    validateHoverState(array, targetIndex, reject,
                       'Hover state must be updated after scroll ends');
    resolve();
  };
  return new Promise((resolve, reject) => {
    requestAnimationFrame(() => { runTest(resolve, reject); });
  });
}

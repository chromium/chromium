var middleClickAutoscrollRadius = 15; // from FrameView::noPanScrollRadius
var waitTimeBeforeMoveInMilliseconds = 100;
var scrollable;
var scrolledObject;
var startX;
var startY;
var endX;
var endY;
var autoscrollParam;
const middleButton = 1;

function $(id) {
  return document.getElementById(id);
}

function testSetUp(param) {
  // Make sure animations run. This requires compositor-controls.js.
  setAnimationRequiresRaster();

  scrollable = param.scrollable;
  scrolledObject = param.scrolledObject || scrollable;
  startX = param.startX || scrollable.offsetLeft + 5;
  startY = param.startY || scrollable.offsetTop + 5;
  endX = param.endX || scrollable.offsetLeft + 5;
  endY = param.endY || scrollable.offsetTop + middleClickAutoscrollRadius + 6;
  autoscrollParam = param;
  if (!scrollable.innerHTML) {
    for (var i = 0; i < 100; ++i) {
      var line = document.createElement('div');
      line.innerHTML = "line " + i;
      scrollable.appendChild(line);
    }
  }
  promise_test (async () => {
    // Wait until layer information has gone from Blink to CC's active tree.
    await waitForCompositorCommit();

    // Start autoscrolling.
    const scrollPromise = waitForScrollEvent(scrolledObject);
    const scrollEndPromise = waitForScrollendEvent(scrolledObject);
    let gesturePromise;
    if (autoscrollParam.clickOrDrag == 'click') {
      gesturePromise = mouseMoveTo(startX, startY)
      .then(mouseClickOn(startX, startY, middleButton))
      .then(mouseMoveTo(endX, endY));
      await Promise.all([scrollPromise, gesturePromise]);
    } else {
      assert_equals('drag', autoscrollParam.clickOrDrag);
      gesturePromise = mouseDragAndDrop(startX, startY, endX, endY, middleButton,
          waitTimeBeforeMoveInMilliseconds);
      await Promise.all([scrollPromise, gesturePromise, scrollEndPromise]);
    }

    if (autoscrollParam.clickOrDrag == 'click') {
      const clickPromise = mouseClickOn(endX, endY, middleButton);
      await Promise.all([clickPromise, scrollEndPromise]);
    }

    assert_true(scrollable.scrollTop > 0 || scrollable.scrollLeft > 0);

    // Wait for the cursor shape to go back to normal.
    await waitUntil(() => {
      var cursorInfo = internals.getCurrentCursorInfo();
      return cursorInfo == "type=Pointer" || cursorInfo == "type=IBeam";
    });

    finishTest();
  });
}

function finishTest() {
  if ($('container'))
    $('container').innerHTML = '';
  if (autoscrollParam.finishTest)
    autoscrollParam.finishTest();
}

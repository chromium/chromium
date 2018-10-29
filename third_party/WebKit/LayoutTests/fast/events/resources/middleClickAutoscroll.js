var middleClickAutoscrollRadius = 15; // from FrameView::noPanScrollRadius
var waitTimeBeforeMoveInSeconds = 0.1;
var scrollable;
var scrolledObject;
var startX;
var startY;
var endX;
var endY;
var autoscrollParam;

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

    // Start atuoscrolling.
    if (autoscrollParam.clickOrDrag == 'click') {
      await mouseMoveTo(startX, startY);
      await mouseClickOn(startX, startY, 'middle');
      await mouseMoveTo(endX, endY);
    } else {
      assert_equals('drag', autoscrollParam.clickOrDrag);
      mouseDragAndDrop(startX, startY, endX, endY, 'middle',
          waitTimeBeforeMoveInSeconds);
    }

    // Wait for some scrolling, then end the autoscroll.
    await waitFor(() => {
      return scrolledObject.scrollTop > 0 || scrolledObject.scrollLeft > 0;
    });
    if (autoscrollParam.clickOrDrag == 'click')
      await mouseClickOn(endX, endY, 'middle');

    // Wait for the cursor shape to go back to normal.
    await waitFor(() => {
      var cursorInfo = internals.getCurrentCursorInfo();
      return cursorInfo == "type=Pointer hotSpot=0,0" ||
          cursorInfo == "type=IBeam hotSpot=0,0";
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

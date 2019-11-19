// This file contains the commonly used functions in pointerevent tests.

const scrollOffset = 20;
const boundaryOffset = 2;

function delayPromise(delay) {
  return new Promise(function(resolve, reject) {
    window.setTimeout(resolve, delay);
  });
}

function scrollPageIfNeeded(targetSelector, targetDocument) {
  var target = targetDocument.querySelector(targetSelector);
  var targetRect = target.getBoundingClientRect();
  if (targetRect.top < 0 || targetRect.left < 0 || targetRect.bottom > window.innerHeight || targetRect.right > window.innerWidth)
    window.scrollTo(targetRect.left, targetRect.top);
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

// Mouse inputs.
function mouseMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions: [{name: 'pointerMove', x: 0, y: 0}]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseMoveIntoTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions:
                [{name: 'pointerMove', x: xPosition, y: yPosition}]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseChordedButtonPress(targetSelector) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      const leftButton = 0;
      const middleButton = 1;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions: [
              {
                name: 'pointerDown',
                x: xPosition,
                y: yPosition,
                button: leftButton
              },
              {
                name: 'pointerDown',
                x: xPosition,
                y: yPosition,
                button: middleButton
              },
              {name: 'pointerUp', button: middleButton},
              {name: 'pointerUp', button: leftButton}
            ]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseClickInTarget(targetSelector, targetFrame, button, shouldScrollToTarget = true) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  // Initialize the button value to left button.
  if (button === undefined) {
    button = 0;
  }
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      if (shouldScrollToTarget)
        scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions: [
              {name: 'pointerMove', x: xPosition, y: yPosition},
              {name: 'pointerDown', x: xPosition, y: yPosition, button: button},
              {name: 'pointerUp', button: button}
            ]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseDragInTargets(targetSelectorList, button) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      // Initialize the button value to left button.
      if (button === undefined) {
        button = 0;
      }
      scrollPageIfNeeded(targetSelectorList[0], document);
      var target = document.querySelector(targetSelectorList[0]);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      var pointerActions = [{'source': 'mouse'}];
      var pointerAction = pointerActions[0];
      pointerAction.actions = [];
      pointerAction.actions.push(
          {name: 'pointerDown', x: xPosition, y: yPosition, button: button});
      for (var i = 1; i < targetSelectorList.length; i++) {
        scrollPageIfNeeded(targetSelectorList[i], document);
        target = document.querySelector(targetSelectorList[i]);
        targetRect = target.getBoundingClientRect();
        xPosition = targetRect.left + boundaryOffset;
        yPosition = targetRect.top + boundaryOffset;
        pointerAction.actions.push(
            {name: 'pointerMove', x: xPosition, y: yPosition, button: button});
      }
      pointerAction.actions.push({name: 'pointerUp', button: button});
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject();
    }
  });
}

function mouseDragInTarget(targetSelector) {
  return mouseDragInTargets([targetSelector, targetSelector]);
}

function mouseWheelScroll(targetSelector, direction) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      eventSender.mouseMoveTo(
          targetRect.left + boundaryOffset, targetRect.top + boundaryOffset);
      eventSender.mouseDown(0);
      eventSender.mouseUp(0);
      if (direction == 'down')
        eventSender.continuousMouseScrollBy(-scrollOffset, 0);
      else if (direction == 'right')
        eventSender.continuousMouseScrollBy(0, -scrollOffset);
      else
        reject();
      resolve();
    } else {
      reject();
    }
  });
}

// Request a pointer lock and capture.
function mouseRequestPointerLockAndCaptureInTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  // Initialize the button value to left button.
  var button = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;

      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'mouse',
         actions: [
            {name: 'pointerMove', x: xPosition, y: yPosition},
            {name: 'pointerDown', x: xPosition, y: yPosition, button: button},
            {name: 'pointerMove', x: xPosition + 30, y: yPosition + 30},
            {name: 'pointerMove', x: xPosition + 30, y: yPosition},
            {name: 'pointerMove', x: xPosition + 60, y: yPosition + 30},
            {name: 'pointerMove', x: xPosition + 30, y: yPosition + 20},
            {name: 'pointerMove', x: xPosition + 10, y: yPosition + 50},
            {name: 'pointerMove', x: xPosition + 40, y: yPosition + 10},
            {name: 'pointerMove', x: xPosition + 10, y: yPosition + 50},
            {name: 'pointerMove', x: xPosition + 40, y: yPosition + 10},
        ]}], resolve);
    } else {
      reject();
    }
  });
}

// Touch inputs.
function touchTapInTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'touch',
         actions: [
            { name: 'pointerDown', x: xPosition, y: yPosition },
            { name: 'pointerUp' }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function pointerDragInTarget(pointerType, targetSelector, direction) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition1 = targetRect.left + boundaryOffset + scrollOffset;
      var yPosition1 = targetRect.top + boundaryOffset + scrollOffset;
      var xPosition2 = xPosition1;
      var yPosition2 = yPosition1;
      var xPosition3 = xPosition1;
      var yPosition3 = yPosition1;
      if (direction == "down") {
        yPosition1 -= scrollOffset;
        yPosition3 += scrollOffset;
      } else if (direction == "up") {
        yPosition1 += scrollOffset;
        yPosition3 -= scrollOffset;
      } else if (direction == "right") {
        xPosition1 -= scrollOffset;
        xPosition3 += scrollOffset;
      } else if (direction == "left") {
        xPosition1 += scrollOffset;
        xPosition3 -= scrollOffset;
      } else {
        throw("drag direction '" + direction + "' is not expected, direction should be 'down', 'up', 'left' or 'right'");
      }

      // Ensure the compositor is aware of any scrolling done in
      // |scrollPageIfNeeded| before sending the input events.
      waitForCompositorCommit().then(() => {
        chrome.gpuBenchmarking.pointerActionSequence( [
          {source: pointerType,
           actions: [
              { name: 'pointerDown', x: xPosition1, y: yPosition1 },
              { name: 'pointerMove', x: xPosition2, y: yPosition2 },
              { name: 'pointerMove', x: xPosition3, y: yPosition3 },
              { name: 'pause', duration: 100 },
              { name: 'pointerUp' }
          ]}], resolve);
      });
    } else {
      reject();
    }
  });
}

function touchScrollInTarget(targetSelector, direction) {
  if (direction == "down")
    direction = "up";
  else if (direction == "up")
    direction = "down";
  else if (direction == "right")
    direction = "left";
  else if (direction == "left")
    direction = "right";
  return pointerDragInTarget('touch', targetSelector, direction);
}

function pinchZoomInTarget(targetSelector, scale) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + (targetRect.width/2);
      var yPosition1 = targetRect.top + (targetRect.height/2) - 10;
      var yPosition2 = targetRect.top + (targetRect.height/2) + 10;
      var pointerActions = [{'source': 'touch'}, {'source': 'touch'}];
      var pointerAction1 = pointerActions[0];
      var pointerAction2 = pointerActions[1];
      pointerAction1.actions = [];
      pointerAction2.actions = [];
      pointerAction1.actions.push(
          {name: 'pointerDown', x: xPosition, y: yPosition1});
      pointerAction2.actions.push(
          {name: 'pointerDown', x: xPosition, y: yPosition2});
      for (var offset = 10; offset < 80; offset += 10) {
        pointerAction1.actions.push({
          name: 'pointerMove',
          x: xPosition,
          y: (yPosition1 - offset)
        });
        pointerAction2.actions.push({
          name: 'pointerMove',
          x: xPosition,
          y: (yPosition2 + offset)
        });
      }
      pointerAction1.actions.push({name: 'pointerUp'});
      pointerAction2.actions.push({name: 'pointerUp'});
      // Ensure the compositor is aware of any scrolling done in
      // |scrollPageIfNeeded| before sending the input events.
      waitForCompositorCommit().then(() => {
        chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
      });
    } else {
      reject();
    }
  });
}

// Pen inputs.
function penMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'pen',
         actions: [
            { name: 'pointerMove', x: 0, y: 0 }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function penMoveIntoTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'pen',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition}
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function penEnterAndLeaveTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }

  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'pen',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition},
            { name: 'pointerLeave' },
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function penClickInTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'pen',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition },
            { name: 'pointerDown', x: xPosition, y: yPosition },
            { name: 'pointerUp' }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

// Drag and drop actions
function mouseDragAndDropInTargets(targetSelectorList) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      scrollPageIfNeeded(targetSelectorList[0], document);
      var target = document.querySelector(targetSelectorList[0]);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      eventSender.mouseMoveTo(xPosition, yPosition);
      eventSender.mouseDown();
      eventSender.leapForward(100);
      for (var i = 1; i < targetSelectorList.length; i++) {
        scrollPageIfNeeded(targetSelectorList[i], document);
        target = document.querySelector(targetSelectorList[i]);
        targetRect = target.getBoundingClientRect();
        xPosition = targetRect.left + boundaryOffset;
        yPosition = targetRect.top + boundaryOffset;
        eventSender.mouseMoveTo(xPosition, yPosition);
      }
      eventSender.mouseUp();
      resolve();
    } else {
      reject();
    }
  });
}

function smoothDrag(targetSelector1, targetSelector2, pointerType) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector1, document);
      var target1 = document.querySelector(targetSelector1);
      var targetRect1 = target1.getBoundingClientRect();
      var xPosition1 = targetRect1.left + boundaryOffset;
      var yPosition1 = targetRect1.top + boundaryOffset;
      var target2 = document.querySelector(targetSelector2);
      var targetRect2 = target2.getBoundingClientRect();
      var xPosition2 = targetRect2.left + boundaryOffset;
      var yPosition2 = targetRect2.top + boundaryOffset;
      var action = '[{"source": "' + pointerType + '", "actions": [{ "name": "pointerDown", "x":' + xPosition1 +', "y":' + yPosition1 +' },';
      var maxStep = Math.max(1, Math.floor(Math.max(Math.abs(xPosition2 - xPosition1), Math.abs(yPosition2 - yPosition1))/15));
      for (var step=1; step<=maxStep; step++)
        action += '{ "name": "pointerMove", "x":' + (xPosition1 + Math.floor((step/maxStep)*(xPosition2 - xPosition1))) +', "y":' + (yPosition1 + Math.floor((step/maxStep)*(yPosition2 - yPosition1))) +' },';
      action += '{ "name": "pointerUp" }]}]';
      chrome.gpuBenchmarking.pointerActionSequence(JSON.parse(action), resolve);
    } else {
      reject();
    }
  });
}

{
  var pointerevent_automation = async_test("PointerEvent Automation");
  // Defined in every test and should return a promise that gets resolved when input is finished.
  inject_input().then(function() {
    pointerevent_automation.done();
  });
}

function keyDown(key, modifiers) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
    eventSender.keyDown(key, modifiers);
    resolve();
    } else {
    reject();
    }
  });
}

const boundaryOffset = 2;

function mouseClickInTarget(targetSelector) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  var button = 0;

  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
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

function inject_input() {
  return mouseClickInTarget("#el").then(() => {
    return keyDown("y", ["ctrlKey"]);
  });
};

inject_input();
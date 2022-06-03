const boundaryOffset = 2;

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

function penEnterAndLeaveTarget(targetSelector) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
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

function inject_input() {
  return penEnterAndLeaveTarget('#target0').then(function() {
    penMoveToDocument();
  }).then(function() {
    return penEnterAndLeaveTarget('#target0');
  });
}

{
  var pointerevent_automation = async_test("PointerEvent Automation");
  // Defined in every test and should return a promise that gets resolved when input is finished.
  inject_input().then(function() {
    pointerevent_automation.done();
  });
}
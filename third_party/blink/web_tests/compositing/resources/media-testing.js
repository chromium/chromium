if (window.testRunner) {
  testRunner.waitUntilDone();
}

var g_swaps_before_success = 5
function waitForSwapsToComplete(firstFrameCallback) {
  if (--g_swaps_before_success > 0) {
    window.requestAnimationFrame(_ => {
      waitForSwapsToComplete(firstFrameCallback);
    });
  } else {
    firstFrameCallback();
  }
}

function setupVideo(videoElement, videoPath, firstFrameCallback) {
  videoElement.requestVideoFrameCallback(_ => {
    waitForSwapsToComplete(firstFrameCallback);
  });
  videoElement.src = videoPath;
}

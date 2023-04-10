if (window.testRunner) {
  testRunner.waitUntilDone();
}

function setupVideo(videoElement, videoPath, firstFrameCallback) {
  videoElement.requestVideoFrameCallback(firstFrameCallback);
  videoElement.src = videoPath;
}

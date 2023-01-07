// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

// Some videos are less than 60 fps, so actual video frame presentations
// could be much less than 30.
var g_swaps_before_success = 30

async function main() {
  video = document.getElementById("video");
  video.loop = true;
  video.requestVideoFrameCallback(waitForVideoToPlay);
  video.srcObject = await navigator.mediaDevices.getUserMedia({video: true});
  video.play();
}

function waitForVideoToPlay() {
  chrome.gpuBenchmarking.addSwapCompletionEventListener(
      waitForSwapsToComplete);
}

function waitForSwapsToComplete() {
  g_swaps_before_success--;
  if (g_swaps_before_success > 0) {
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  } else {
    domAutomationController.send("SUCCESS");
  }
}

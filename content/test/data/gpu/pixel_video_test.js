// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

// Some videos are less than 60 fps, so actual video frame presentations
// could be much less than 30.
var g_swaps_before_success = 30

function main() {
  video = document.getElementById("video");
  video.loop = true;
  video.addEventListener('timeupdate', waitForVideoToPlay);
  video.play();
}

function waitForVideoToPlay() {
  if (video.currentTime > 0) {
    video.removeEventListener('timeupdate', waitForVideoToPlay);
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  }
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

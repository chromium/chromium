// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

// Some videos are less than 60 fps, so actual video frame presentations
// could be much less than 30.
var g_swaps_before_success = 30;

var abort = false;

function main() {
  video = document.getElementById('video');
  video.loop = true;
  video.muted = true;  // No need to exercise audio paths.

  video.onerror = _ => {
    abort = true;
    domAutomationController.send('FAIL');
  };

  video.play();

  // These tests expect playback, so we intentionally don't request the frame
  // callback before starting playback. Since these videos loop there should
  // always be frames being generated.
  video.requestVideoFrameCallback(_ => {
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  });
}

function waitForSwapsToComplete() {
  if (abort)
    return;

  g_swaps_before_success--;
  if (g_swaps_before_success > 0) {
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  } else {
    domAutomationController.send('SUCCESS');
  }
}

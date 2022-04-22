// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

// Some videos are less than 60 fps, so actual video frame presentations
// could be much less than 30.
var g_swaps_before_success = 30;

var abort = false;

function logOutput(s) {
  if (window.domAutomationController)
    window.domAutomationController.log(s);
  else
    console.log(s);
}

function main() {
  video = document.getElementById('video');
  video.loop = true;
  video.muted = true;  // No need to exercise audio paths.

  video.onerror = e => {
    logOutput(`Test failed: ${e.message}`);
    abort = true;
    domAutomationController.send('FAIL');
  };

  logOutput('Playback started.');
  video.play();

  // These tests expect playback, so we intentionally don't request the frame
  // callback before starting playback. Since these videos loop there should
  // always be frames being generated.
  video.requestVideoFrameCallback((_, f) => {
    logOutput(`First frame: ${f.width}x${f.height}, ts: ${f.mediaTime}`);
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  });
}

function waitForSwapsToComplete() {
  if (abort)
    return;

  g_swaps_before_success--;
  if (g_swaps_before_success > 0) {
    if (g_swaps_before_success % 5 == 0) {
      logOutput(
          `Waiting for swaps to complete: ${g_swaps_before_success} left.`);
    }
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  } else {
    logOutput('Test complete.');
    domAutomationController.send('SUCCESS');
  }
}

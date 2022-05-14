// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;
var abort = false;

// The time waiting for collecting overlay presentation statistics.
// 500 ms is supposed to be 15 frames at 30 Hz. However, only ~7 frames or less
// are recorded in OverlayModeTraceTest_DirectComposition tests.
// Reducing this delay might cause flakes in some tests.
var delayMs = 1000;

function logOutput(s) {
  if (window.domAutomationController)
    window.domAutomationController.log(s);
  else
    console.log(s);
}

const parsedString = (function (names) {
  const pairs = {};
  for (let i = 0; i < names.length; ++i) {
    var keyValue = names[i].split('=', 2);
    if (keyValue.length == 1)
      pairs[keyValue[0]] = '';
    else
      pairs[keyValue[0]] = decodeURIComponent(keyValue[1].replace(/\+/g, ' '));
  }
  return pairs;
})(window.location.search.substr(1).split('&'));

function getTimeDelay() {
  let delayString = parsedString['delayMs'];
  if (delayString != undefined)
    delayMs = delayString;

  return delayMs;
}

function setVideoSize() {
  let width = '240';
  let height = '135';

  let widthString = parsedString['width'];
  let heightString = parsedString['height'];
  if (widthString != undefined)
    width = widthString;
  if (heightString != undefined)
    height = heightString;

  video.width = width;
  video.height = height;
}

function main() {
  let t0Ms = performance.now();
  video = document.getElementById('video');
  video.loop = true;
  video.muted = true;  // No need to exercise audio paths.
  setVideoSize(video);

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
    let elapsedMs = performance.now() - t0Ms;
    logOutput(`First VideoFrameCallback: ${f.width}x${f.height}, video ts: ${
        f.mediaTime}, TimeStamp:${elapsedMs}ms`);

    // Make sure a few frames have be swapped completely.
    // add "--enable-features=ReportFCPOnlyOnSuccessfulCommit" along with
    // gpu_benchmarking to ensure completion event is triggered only on
    // a succdessful commit.
    chrome.gpuBenchmarking.addSwapCompletionEventListener(
        waitForSwapsToComplete);
  });

  let g_swaps_completion = 4;
  function waitForSwapsToComplete() {
    g_swaps_completion--;
    if (g_swaps_completion >= 0) {
      chrome.gpuBenchmarking.addSwapCompletionEventListener(
          waitForSwapsToComplete);
    } else {
      let elapsedMs = performance.now() - t0Ms;
      logOutput(`TimeStamp after 4 swaps:${elapsedMs}ms `);
      // Trace tests on Windows need some time to collect statistics from the
      // overlay system, so allow delay before verifying the stats. A 1000ms is
      // about ~30 swaps at 30Hz.
      setTimeout(_ => {
        if (abort)
          return;
        elapsedMs = performance.now() - t0Ms;
        logOutput(`Test complete. TimeStamp:${elapsedMs}ms`);
        domAutomationController.send('SUCCESS');
      }, getTimeDelay());
    }
  }
}
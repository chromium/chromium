// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;
var totalVideoSwaps = 8;
var useTimer = 0;
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
  logOutput(`Video size:${video.width}x${video.height}`);
}

function getParametersTesting() {
  let swapsString = parsedString['swaps'];
  if (swapsString != undefined)
    totalVideoSwaps = swapsString;

  let useTimerString = parsedString['use_timer'];
  if (useTimerString != undefined)
    useTimer = useTimerString;
}

function main() {
  let t0Ms = performance.now();
  video = document.getElementById('video');
  video.loop = true;
  video.muted = true;  // No need to exercise audio paths.
  setVideoSize(video);
  getParametersTesting();

  video.onerror = e => {
    logOutput(`Video playback error occurred: ${e.message}`);
    abort = true;
    domAutomationController.send('FAIL');
  };

  logOutput('Playback started.');
  video.play().catch(e => {
    logOutput(`play() failed: ${e.message}`);
    domAutomationController.send('FAIL');
  });

  // Used by the swap counter, without using the timer.
  let testCompletion = false;
  let videoFrameReady = false;
  let swapCount = 0;

  // These tests expect playback, so we intentionally don't request the frame
  // callback before starting playback. Since these videos loop there should
  // always be frames being generated.
  video.requestVideoFrameCallback((_, f) => {
    let timestamp = performance.now() - t0Ms;
    logOutput(`First videoFrameCallback: TimeStamp:${timestamp}ms`);

    if (useTimer == 1) {
      setTimeout(_ => {
        logOutput('Test complete.');
        domAutomationController.send('SUCCESS');
      }, delayMs);
    } else {
      video.requestVideoFrameCallback(rVF_function);
      chrome.gpuBenchmarking.addSwapCompletionEventListener(
          waitForSwapsToComplete);
    }
  });


  function rVF_function() {
    videoFrameReady = true;
    if (!testCompletion) {
      video.requestVideoFrameCallback(rVF_function);
    }
  }

  // Must add "--enable-features=ReportFCPOnlyOnSuccessfulCommit" with
  // gpu_benchmarking to ensure completion event callback in
  // addSwapCompletionEventListener is sent only on a succdessful commit.
  function waitForSwapsToComplete() {
    if (videoFrameReady) {
      if (swapCount == 0) {
        let timestamp = performance.now() - t0Ms;
        logOutput(`First video overlay swap: TimeStamp:${timestamp}ms`);
      }

      swapCount++;
      videoFrameReady = false;
    }

    if (swapCount < totalVideoSwaps) {
      chrome.gpuBenchmarking.addSwapCompletionEventListener(
          waitForSwapsToComplete);
    } else {
      testCompletion = true;
      let timestamp = performance.now() - t0Ms;
      logOutput(`Total swaps: ~${totalVideoSwaps}. Timestamp:${timestamp}ms`);
      logOutput('Test complete.');

      domAutomationController.send('SUCCESS');
    }
  }
}

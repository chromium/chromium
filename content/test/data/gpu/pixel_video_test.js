// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;
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

    // Trace tests on Windows need some time to collect statistics from the
    // overlay system, so allow for a 500ms delay (~30 swaps at 60Hz).
    setTimeout(_ => {
      if (abort)
        return;
      logOutput('Test complete.');
      domAutomationController.send('SUCCESS');
    }, 500);
  });
}
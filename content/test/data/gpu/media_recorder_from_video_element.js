// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var srcVideo;
var dstVideo;
let recorder = null;
let stream = null;

// Note: This mime type is from the repro case for crbug.com/1360531
// It is not clear whether it would be worthwhile to run this test
// using a multitude of format/codec combinations.
const mimeType = 'video/webm;codecs=vp8';

const chunks = [];

function logOutput(s) {
  if (window.domAutomationController) {
    window.domAutomationController.log(s);
  } else {
    console.log(s);
  }
}

function sendResult(status) {
  if (window.domAutomationController) {
    window.domAutomationController.send(status);
  } else {
    console.log(status);
  }
}

function setVideoSize() {
  const width = '240';
  const height = '135';
  srcVideo.width = width;
  srcVideo.height = height;
  dstVideo.width = width;
  dstVideo.height = height;
}

function startPlayback() {
  logOutput('Preparing playback.');
  var blob = new Blob(chunks, { 'type': mimeType });
  var videoURL = window.URL.createObjectURL(blob);
  dstVideo.onended = function() {
    logOutput('Playback complete.');
    sendResult('SUCCESS');
  }
  dstVideo.onerror = _ => {
    logOutput(`Test failed: ${dstVideo.error.message}`);
    abort = true;
    sendResult('FAIL');
  };
  dstVideo.src = videoURL;
  dstVideo.play();
  logOutput('Playback started.');
}

function startRecording() {
  logOutput('crbug.com/476172416: startRecording() called');
  stream = srcVideo.captureStream(30);
  logOutput('crbug.com/476172416: Stream captured');
  recorder = new MediaRecorder(stream, { mimeType });
  logOutput('crbug.com/476172416: MediaRecorder created');
  recorder.onstop = startPlayback;
  recorder.ondataavailable = (e) => {
    logOutput(`Recorder data available. ${e.data.size}`);
    chunks.push(e.data);
    if (e.data.size > 50) {
      // We actually got a real encoded video data chunk.
      recorder.ondataavailable = null;
      stopRecording();
    }
  };

  // Start recording and ask it to emit encoded data every 100 ms.
  logOutput('crbug.com/476172416: Starting recorder');
  recorder.start(100);
  logOutput('crbug.com/476172416: Playing video');
  srcVideo.play();

  logOutput('Recording started.');
}

function stopRecording() {
  recorder.stop();
  srcVideo.pause();
  logOutput('Recording stopped.');
}

function main() {
  logOutput('Test started');
  srcVideo = document.getElementById('src-video');
  srcVideo.loop = true;
  srcVideo.muted = true;  // No need to exercise audio paths.
  dstVideo = document.getElementById('dst-video');
  dstVideo.loop = false;
  dstVideo.muted = true;  // No need to exercise audio paths.
  logOutput('crbug.com/476172416: Setting video size');
  setVideoSize();

  srcVideo.onerror = _ => {
    logOutput(`Test failed: ${dstVideo.error.message}`);
    abort = true;
    sendResult('FAIL');
  };

  if (srcVideo.readyState >= HTMLMediaElement.HAVE_FUTURE_DATA) {
    logOutput('crbug.com/476172416: Starting recording immediately');
    startRecording();
  } else {
    logOutput(
        'crbug.com/476172416: Starting recording after more data is loaded');
    srcVideo.oncanplaythrough = startRecording;
  }
}

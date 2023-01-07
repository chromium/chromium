// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stream;

function success(s) {
  stream = s;
  common.naclModule.postMessage({
    command: 'init',
    track: stream.getVideoTracks()[0]
  });
}

function failure(e) {
  common.logMessage("Error: " + e);
}

function changeFormat(format) {
  common.naclModule.postMessage({command:'format', format: format});
}

function changeSize(width, height) {
  common.naclModule.postMessage({command:'size', width: width, height: height});
}

function moduleDidLoad() {
  navigator.webkitGetUserMedia({'video': true}, success, failure);
}

function attachListeners() {
  document.getElementById('YV12').addEventListener(
      'click', function() { changeFormat('YV12'); });
  document.getElementById('I420').addEventListener(
      'click', function() { changeFormat('I420'); });
  document.getElementById('BGRA').addEventListener(
      'click', function() { changeFormat('BGRA'); });
  document.getElementById('DEFAULT').addEventListener(
      'click', function() { changeFormat('DEFAULT'); });

  document.getElementById('S72X72').addEventListener(
      'click', function() { changeSize(72, 72); });
  document.getElementById('S640X360').addEventListener(
      'click', function() { changeSize(640, 360); });
  document.getElementById('S1280X720').addEventListener(
      'click', function() { changeSize(1280, 720); });
  document.getElementById('SDEFAULT').addEventListener(
      'click', function() { changeSize(0, 0); });

}

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stream;

function success(s) {
  stream = s;
  common.naclModule.postMessage({track: stream.getAudioTracks()[0]});
}

function failure(e) {
  common.logMessage("Error: " + e);
}

function moduleDidLoad() {
  navigator.webkitGetUserMedia({'audio': true}, success, failure);
}

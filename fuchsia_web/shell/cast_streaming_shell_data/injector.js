// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// JavaScript test harness to be injected into receiver.html.

var domAutomationController = {};
domAutomationController._frame_count = 0;
domAutomationController._frame_request = 0;

domAutomationController._done = false;
domAutomationController._failure = false;

// Waits for document to be fully loaded before calling
// requestVideoFrameCallback.
document.onreadystatechange = function() {
  if (document.readyState === 'complete') {
    const video = document.querySelector('video');
    if (!video) {
      console.log('Video element could not be found');
      window.close();
      return;
    }
    video.requestVideoFrameCallback(function(now, metadata) {
      // Increments frame count when a frame is presented for composition.
      domAutomationController._frame_count++;
      domAutomationController.checkTermination();
    });
  }
}

// Checks termination condition by comparing frame requests and frame count of
// received frames.
domAutomationController.checkTermination = function() {
  if (this._frame_request === 0) {
    return;
  }
  if (this._frame_request >= this._frame_count) {
    this._done = true;
  }
}

// Tracks frame requests.
domAutomationController.setFrameRequest = function(frame_request) {
  this._frame_request = frame_request;
  this.checkTermination();
}

// Sets failure flag on window.close, called on ended or error events.
window.close = function() {
  domAutomationController._failure = true;
  domAutomationController._done = true;
};

window.domAutomationController = domAutomationController;

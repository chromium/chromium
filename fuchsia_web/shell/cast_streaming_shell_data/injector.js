// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// JavaScript test harness to be injected into receiver.html.

var domAutomationController = {};
domAutomationController._frame_count = 0;
domAutomationController._frame_request = 0;

domAutomationController._done = false;
domAutomationController._failure = false;

// Waits for video element to be loaded before calling
// requestVideoFrameCallback.
var observer = new MutationObserver(function(mutationList, observer) {
  for (const { addedNodes } of mutationList) {
    for (const node of addedNodes) {
      if (node.nodeName == "VIDEO") {
        var video = document.querySelector("video");
        video.requestVideoFrameCallback(domAutomationController.addFrame);
        observer.disconnect();
        break;
      }
    }
  }
});
observer.observe(document, { childList: true, subtree: true });

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

// Increments frame count when a frame is presented for composition.
domAutomationController.addFrame = function(now, metadata) {
  domAutomationController._frame_count++;
  domAutomationController.checkTermination();

  var video = document.querySelector('video');
  video.requestVideoFrameCallback(domAutomationController.addFrame);
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

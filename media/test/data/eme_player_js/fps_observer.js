// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FPSObserver observes a <video> and reports decoded FPS, dropped FPS, and
// total dropped frames during the video playback.
var FPSObserver = new function() {
  this.video_ = null;
  this.decodedFrames_ = 0;
  this.droppedFrames_ = 0;
  this.startTime_ = 0;
  this.intID_ = null;
}

FPSObserver.observe = function(video) {
  this.video_ = video;
  var observer = this;
  this.video_.addEventListener('playing', function() {
    observer.onVideoPlaying();
  });

  this.video_.addEventListener('error', function() {
    observer.endTest();
  });

  this.video_.addEventListener('ended', function() {
    observer.endTest();
  });
};

FPSObserver.onVideoPlaying = function() {
  this.decodedFrames_ = 0;
  this.droppedFrames_ = 0;
  this.startTime_ = window.performance.now();
  this.endTest(true);
  var observer = this;
  this.intID_ = window.setInterval(function() {
      observer.calculateStats();}, 1000);
};

FPSObserver.calculateStats = function() {
  if (this.video_.readyState <= HTMLMediaElement.HAVE_CURRENT_DATA ||
      this.video_.paused || this.video_.ended)
    return;
  var currentTime = window.performance.now();
  var deltaTime = (currentTime - this.startTime_) / 1000;
  this.startTime_ = currentTime;

  // Calculate decoded frames per sec.
  var fps = (this.video_.webkitDecodedFrameCount - this.decodedFrames_) /
             deltaTime;
  this.decodedFrames_ = this.video_.webkitDecodedFrameCount;
  fps = fps.toFixed(2);
  decodedFPSElement.innerHTML = fps;

  // Calculate dropped frames per sec.
  fps = (this.video_.webkitDroppedFrameCount - this.droppedFrames_) / deltaTime;
  this.droppedFrames_ = this.video_.webkitDroppedFrameCount;
  fps = fps.toFixed(2);
  droppedFPSElement.innerHTML = fps;

  droppedFramesElement.innerHTML = this.droppedFrames_;
};

FPSObserver.endTest = function() {
  window.clearInterval(this.intID_);
};

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSEPlayerUtils provides functionality to load and append media segments using
// MSE API. The segments can be fetched at specified intervals to simulate
// generic MSE video players.
class MSEPlayerUtilsClass {
  loadMediaSegmentsFromTestConfig(testConfig) {
    this.mediaSegments = testConfig.mediaFile;
    this.mediaType = testConfig.mediaType;
    // Duration for each of the media segment.
    this.MSESegmentDurationMS = testConfig.MSESegmentDurationMS
    // How long before media play end should the next segment be fetched.
    this.MSESegmentFetchDelayBeforeEndMS =
      testConfig.MSESegmentFetchDelayBeforeEndMS
    if (!this.mediaSegments || !Array.isArray(this.mediaSegments)) {
      Utils.failTest('Missing mediaSegments.');
      return;
    }
    if (!this.mediaType) {
      Utils.failTest('Missing mediaType.');
      return;
    }
    if (this.MSESegmentDurationMS <= 0) {
      Utils.failTest('Invalid MSESegmentDurationMS.');
      return;
    }
    if (this.MSESegmentFetchDelayBeforeEndMS <= 0) {
      Utils.failTest('Invalid MSESegmentFetchDelayBeforeEndMS.');
      return;
    }
    this.mediaSource = new MediaSource();
    this.video = document.querySelector("video");
    this.video.src = URL.createObjectURL(this.mediaSource);
    this.mediaSource.addEventListener("sourceopen",
      this.onMediaSourceOpen.bind(this));
    this.currentSegmentIndex = 0
  }

  onMediaSourceOpen() {
    this.sourceBuffer = this.mediaSource.addSourceBuffer(this.mediaType);
    this.sourceBuffer.addEventListener("updateend",
      this.onSourceBufferUpdateEnd.bind(this));
    // Trigger the first segment fetch
    this.fetchNextSegmentAndAppend();
  }

  onSourceBufferUpdateEnd() {
    this.sourceBuffer.timestampOffset += this.MSESegmentDurationMS/1000;
    // The next segment should be fetched MSESegmentFetchDelayBeforeEndMS millis
    // before the end of video play.
    const total_duration = this.currentSegmentIndex*this.MSESegmentDurationMS;
    setTimeout(this.fetchNextSegmentAndAppend.bind(this),
      total_duration - (1000*this.video.currentTime) -
      this.MSESegmentFetchDelayBeforeEndMS)
  }

  fetchNextSegmentAndAppend() {
    if (this.currentSegmentIndex >= this.mediaSegments.length) {
      this.mediaSource.endOfStream()
      return;
    }
    const sourceBuffer = this.sourceBuffer;
    const xhr = new XMLHttpRequest();
    xhr.open('GET', this.mediaSegments[this.currentSegmentIndex++]);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('load', function() {
      sourceBuffer.appendBuffer(xhr.response);
    });
    xhr.send();
  }
}

const MSEPlayerUtils = new MSEPlayerUtilsClass();

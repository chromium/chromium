/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*jshint esversion: 6 */

/**
 * A loopback peer connection with one or more streams.
 */
class PeerConnection {
  /**
   * Creates a loopback peer connection. One stream per supplied resolution is
   * created.
   * @param {!Element} videoElement the video element to render the feed on.
   * @param {!Array<!{x: number, y: number}>} resolutions. A width of -1 will
   *     result in disabled video for that stream.
   */
  constructor(videoElement, resolutions) {
    this.localConnection = null;
    this.remoteConnection = null;
    this.remoteView = videoElement;
    this.streams = [];
    // Ensure sorted in descending order to conveniently request the highest
    // resolution first through GUM later.
    this.resolutions = resolutions.slice().sort((x, y) => y.w - x.w);
    this.activeStreamIndex = resolutions.length - 1;
    this.badResolutionsSeen = 0;
    this.localAudioTransceiver = null;
    this.localVideoTransceiver = null;
  }

  /**
   * Starts the connections. Triggers GetUserMedia and starts
   * to render the video on {@code this.videoElement}.
   * @return {!Promise} a Promise that resolves when everything is initalized.
   */
  async start() {
    // getUserMedia fails if we first request a low resolution and
    // later a higher one. Hence, sort resolutions above and
    // start with the highest resolution here.
    const promises = this.resolutions.map((resolution) => {
      const constraints = createMediaConstraints(resolution);
      return navigator.mediaDevices
        .getUserMedia(constraints)
        .then((stream) => this.streams.push(stream));
    });
    await Promise.all(promises);
    // Start with the smallest video to not overload the machine instantly.
    await this.createPeerConnections_(this.streams[this.activeStreamIndex]);
  };

  /**
   * Verifies that the state of the streams are good. The state is good if all
   * streams are active and their video elements report the resolution the
   * stream is in. Video elements are allowed to report bad resolutions
   * numSequentialBadResolutionsForFailure times before failure is reported
   * since video elements occasionally report bad resolutions during the tests
   * when we manipulate the streams frequently.
   * @param {number=} numSequentialBadResolutionsForFailure number of bad
   *     resolution observations in a row before failure is reported.
   * @param {number=} allowedDelta allowed difference between expected and
   *     actual resolution. We have seen videos assigned a resolution one pixel
   *     off from the requested.
   */
  verifyState(numSequentialBadResolutionsForFailure=10, allowedDelta=1) {
    this.verifyAllStreamsActive_();
    const expectedResolution = this.resolutions[this.activeStreamIndex];
    if (expectedResolution.w < 0 || expectedResolution.h < 0) {
      // Video is disabled.
      return;
    }
    if (!isWithin(
            this.remoteView.videoWidth, expectedResolution.w, allowedDelta) ||
        !isWithin(
            this.remoteView.videoHeight, expectedResolution.h, allowedDelta)) {
      this.badResolutionsSeen++;
    } else if (
        this.badResolutionsSeen < numSequentialBadResolutionsForFailure) {
      // Reset the count, but only if we have not yet reached the limit. If the
      // limit is reached, let keep the error state.
      this.badResolutionsSeen = 0;
    }
    if (this.badResolutionsSeen >= numSequentialBadResolutionsForFailure) {
      throw new Error(
          'Expected video resolution ' +
          resStr(expectedResolution.w, expectedResolution.h) +
          ' but got another resolution ' + this.badResolutionsSeen +
          ' consecutive times. Last resolution was: ' +
          resStr(this.remoteView.videoWidth, this.remoteView.videoHeight));
    }
  }

  verifyAllStreamsActive_() {
    if (this.streams.some((x) => !x.active)) {
      throw new Error('At least one media stream is not active')
    }
  }

  /**
   * Switches to a random stream, i.e., use a random resolution of the
   * resolutions provided to the constructor.
   * @return {!Promise} A promise that resolved when everything is initialized.
   */
  async switchToRandomStream() {
    await this.stopSending_();
    const newStreamIndex = Math.floor(Math.random() * this.streams.length);
    await this.startSending_(this.streams[newStreamIndex]);
    this.activeStreamIndex = newStreamIndex;
  }

  async createPeerConnections_(stream) {
    this.localConnection = new RTCPeerConnection();
    this.localConnection.onicecandidate = (event) => {
      this.onIceCandidate_(this.remoteConnection, event);
    };
    this.remoteConnection = new RTCPeerConnection();
    this.remoteConnection.onicecandidate = (event) => {
      this.onIceCandidate_(this.localConnection, event);
    };
    this.remoteConnection.ontrack = (e) => {
      this.remoteView.srcObject = e.streams[0];
    };

    const [audioTrack] = stream.getAudioTracks();
    const [videoTrack] = stream.getVideoTracks();
    this.localAudioTransceiver =
        audioTrack ? this.localConnection.addTransceiver(audioTrack) : null;
    this.localVideoTransceiver =
        videoTrack ? this.localConnection.addTransceiver(videoTrack) : null;
    await this.renegotiate_();
  }

  async startSending_(stream) {
    const [audioTrack] = stream.getAudioTracks();
    const [videoTrack] = stream.getVideoTracks();
    if (audioTrack) {
      await this.localAudioTransceiver.sender.replaceTrack(audioTrack);
      this.localAudioTransceiver.direction = 'sendrecv';
    }
    if (videoTrack) {
      await this.localVideoTransceiver.sender.replaceTrack(videoTrack);
      this.localVideoTransceiver.direction = 'sendrecv';
    }
    await this.renegotiate_();
  }

  async stopSending_() {
    await this.localAudioTransceiver.sender.replaceTrack(null);
    this.localAudioTransceiver.direction = 'inactive';
    await this.localVideoTransceiver.sender.replaceTrack(null);
    this.localVideoTransceiver.direction = 'inactive';
    await this.renegotiate_();
  }

  async renegotiate_() {
    // Implicitly creates the offer.
    await this.localConnection.setLocalDescription();
    await this.remoteConnection.setRemoteDescription(
        this.localConnection.localDescription);
    // Implicitly creates the answer.
    await this.remoteConnection.setLocalDescription();
    await this.localConnection.setRemoteDescription(
        this.remoteConnection.localDescription);
  };

  onIceCandidate_(connection, event) {
    if (event.candidate) {
      connection.addIceCandidate(new RTCIceCandidate(event.candidate));
    }
  };
}

/**
 * Checks if a value is within an expected value plus/minus a delta.
 * @param {number} actual
 * @param {number} expected
 * @param {number} delta
 * @return {boolean}
 */
function isWithin(actual, expected, delta) {
  return actual <= expected + delta && actual >= actual - delta;
}

/**
 * Creates constraints for use with GetUserMedia.
 * @param {!{x: number, y: number}} widthAndHeight Video resolution.
 */
function createMediaConstraints(widthAndHeight) {
  let constraint;
  if (widthAndHeight.w < 0) {
    constraint = false;
  } else {
    constraint = {
      width: {exact: widthAndHeight.w},
      height: {exact: widthAndHeight.h}
    };
  }
  return {
    audio: true,
    video: constraint
  };
}

function resStr(width, height) {
  return `${width}x${height}`
}

function logError(err) {
  console.error(err);
}

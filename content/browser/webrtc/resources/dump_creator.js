// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

/** A list of getUserMedia requests. */
export const userMediaRequests = [];
/** A map from peer connection id to the PeerConnectionRecord. */
export const peerConnectionDataStore = {};

// Also duplicating on window since tests access these from C++.
window.userMediaRequests = userMediaRequests;
window.peerConnectionDataStore = peerConnectionDataStore;

/**
 * Provides the UI for dump creation.
 */
export class DumpCreator {
  /**
   * @param {Element} containerElement The parent element of the dump creation
   *     UI.
   */
  constructor(containerElement) {
    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    this.createDumpRoot(containerElement);
    this.createAudioRecordingRoot(containerElement);
    this.createPacketRecordingRoot(containerElement);
  }

  createDumpRoot(containerElement) {
    this.dumpRoot_ = document.createElement('details');

    this.dumpRoot_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.dumpRoot_);
    const summary = document.createElement('summary');
    this.dumpRoot_.appendChild(summary);
    summary.textContent = 'Create a WebRTC-Internals dump';
    const content = document.createElement('div');
    this.dumpRoot_.appendChild(content);
    content.appendChild($('dump-template').content.cloneNode(true));
    content.getElementsByTagName('a')[0].addEventListener(
      'click', this.onDownloadData_.bind(this));
  }

  createAudioRecordingRoot(containerElement) {
    this.audioRoot_ = document.createElement('details');

    this.audioRoot_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.audioRoot_);
    const summary = document.createElement('summary');
    this.audioRoot_.appendChild(summary);
    summary.textContent = 'Create diagnostic audio recordings';
    const content = document.createElement('div');
    this.audioRoot_.appendChild(content);
    content.appendChild($('audio-recording-template').content.cloneNode(true));
    content.getElementsByTagName('input')[0].addEventListener(
      'click', this.onAudioDebugRecordingsChanged_.bind(this));

  }

  createPacketRecordingRoot(containerElement) {
    this.packetRoot_ = document.createElement('details');

    this.packetRoot_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.packetRoot_);
    const summary = document.createElement('summary');
    this.packetRoot_.appendChild(summary);
    summary.textContent = 'Create diagnostic packet recordings';
    const content = document.createElement('div');
    this.packetRoot_.appendChild(content);
    content.appendChild($('packet-recording-template').content.cloneNode(true));
    content.getElementsByTagName('input')[0].addEventListener(
        'click', this.onEventLogRecordingsChanged_.bind(this));
  }

  // Mark the diagnostic audio recording checkbox checked.
  setAudioDebugRecordingsCheckbox() {
    this.audioRoot_.getElementsByTagName('input')[0].checked = true;
  }

  // Mark the diagnostic audio recording checkbox unchecked.
  clearAudioDebugRecordingsCheckbox() {
    this.audioRoot_.getElementsByTagName('input')[0].checked = false;
  }

  // Mark the event log recording checkbox checked.
  setEventLogRecordingsCheckbox() {
    this.packetRoot_.getElementsByTagName('input')[0].checked = true;
  }

  // Mark the event log recording checkbox unchecked.
  clearEventLogRecordingsCheckbox() {
    this.packetRoot_.getElementsByTagName('input')[0].checked = false;
  }

  // Mark the event log recording checkbox as mutable/immutable.
  setEventLogRecordingsCheckboxMutability(mutable) {
    this.packetRoot_.getElementsByTagName('input')[0].disabled = !mutable;
    if (!mutable) {
      const label = this.packetRoot_.getElementsByTagName('label')[0];
      label.style = 'color:red;';
      label.textContent =
          ' WebRTC event logging\'s state was set by a command line flag.';
    }
  }

  /**
   * Downloads the PeerConnection updates and stats data as a file.
   *
   * @private
   */
  async onDownloadData_(event) {
    const useCompression = this.dumpRoot_.getElementsByTagName('input')[0].checked;
    const dumpObject = {
      'getUserMedia': userMediaRequests,
      'PeerConnections': peerConnectionDataStore,
      'UserAgent': navigator.userAgent,
    };
    const textBlob =
      new Blob([JSON.stringify(dumpObject, null, 1)], {type: 'octet/stream'});
    let url;
    if (useCompression) {
      const compressionStream = new CompressionStream('gzip');
      const binaryStream = textBlob.stream().pipeThrough(compressionStream);
      const binaryBlob = await new Response(binaryStream).blob();
      url = URL.createObjectURL(binaryBlob);
      // Since this is async we can't use the default event and need to click
      // again (while avoiding an infinite loop).
      const anchor = document.createElement('a');
      anchor.download = 'webrtc_internals_dump.gz'
      anchor.href = url;
      anchor.click();
      return;
    }
    url = URL.createObjectURL(textBlob);
    const anchor = this.dumpRoot_.getElementsByTagName('a')[0];
    anchor.download = 'webrtc_internals_dump.txt'
    anchor.href = url;
    // The default action of the anchor will download the url.
  }

  /**
   * Handles the event of toggling the audio debug recordings state.
   *
   * @private
   */
  onAudioDebugRecordingsChanged_() {
    const enabled = this.audioRoot_.getElementsByTagName('input')[0].checked;
    if (enabled) {
      chrome.send('enableAudioDebugRecordings');
    } else {
      chrome.send('disableAudioDebugRecordings');
    }
  }

  /**
   * Handles the event of toggling the event log recordings state.
   *
   * @private
   */
  onEventLogRecordingsChanged_() {
    const enabled = this.packetRoot_.getElementsByTagName('input')[0].checked;
    if (enabled) {
      chrome.send('enableEventLogRecordings');
    } else {
      chrome.send('disableEventLogRecordings');
    }
  }
}

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
  constructor(ignoredContainerElement) {
    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    document.getElementById('dump-click-target').addEventListener(
        'click', this.onDownloadData_.bind(this));
    document.getElementById('audio-recording-click-target').addEventListener(
        'click', this.onAudioDebugRecordingsChanged_.bind(this));
    document.getElementById('packet-recording-click-target').addEventListener(
        'click', this.onEventLogRecordingsChanged_.bind(this));
    document.getElementById('datachannel-recording-click-target')
        .addEventListener(
            'click', this.onDataChannelRecordingsChanged_.bind(this));
  }

  // Mark the diagnostic audio recording checkbox checked.
  setAudioDebugRecordingsCheckbox() {
    document.getElementById('audio-recording-checkbox').checked = true;
  }

  // Mark the diagnostic audio recording checkbox unchecked.
  clearAudioDebugRecordingsCheckbox() {
    document.getElementById('audio-recording-checkbox').checked = false;
  }

  // Mark the event log recording checkbox checked.
  setEventLogRecordingsCheckbox() {
    document.getElementById('packet-recording-checkbox').checked = true;
  }

  // Mark the event log recording checkbox unchecked.
  clearEventLogRecordingsCheckbox() {
    document.getElementById('packet-recording-checkbox').checked = false;
  }

  // Mark the event log recording checkbox as mutable/immutable.
  setEventLogRecordingsCheckboxMutability(mutable) {
    document.getElementById('packet-recording-checkbox').disabled = !mutable;
    if (!mutable) {
      const label = document.getElementById('packet-recording-label');
      label.style = 'color:red;';
      label.textContent =
          ' WebRTC event logging\'s state was set by a command line flag.';
    }
  }

  setDataChannelRecordingsCheckbox() {
    document.getElementById('datachannel-recording-checkbox').checked = true;
  }

  clearDataChannelRecordingsCheckbox() {
    document.getElementById('datachannel-recording-checkbox').checked = false;
  }

  /**
   * Downloads the PeerConnection updates and stats data as a file.
   *
   * @private
   */
  async onDownloadData_(event) {
    const useCompression = document.getElementById('dump-checkbox').checked;
    const uaData = await navigator.userAgentData
        .getHighEntropyValues(['fullVersionList']);
    const dumpObject = {
      'getUserMedia': userMediaRequests,
      'PeerConnections': peerConnectionDataStore,
      'UserAgent': navigator.userAgent,
      'UserAgentData': uaData.fullVersionList,
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
    const anchor = document.createElement('a');
    anchor.download = 'webrtc_internals_dump.txt'
    anchor.href = url;
    anchor.click();
  }

  /**
   * Handles the event of toggling the audio debug recordings state.
   *
   * @private
   */
  onAudioDebugRecordingsChanged_() {
    const checkbox = document.getElementById('audio-recording-checkbox');
    chrome.send((checkbox.checked ? 'en' : 'dis') + 'ableAudioDebugRecordings');
  }

  /**
   * Handles the event of toggling the event log recordings state.
   *
   * @private
   */
  onEventLogRecordingsChanged_() {
    const checkbox = document.getElementById('packet-recording-checkbox');
    chrome.send((checkbox.checked ? "en" : "dis") + 'ableEventLogRecordings');
  }

  /**
   * Handles the event of toggling the event log recordings state.
   *
   * @private
   */
  onDataChannelRecordingsChanged_() {
    const checkbox = document.getElementById('datachannel-recording-checkbox');
    chrome.send(
        (checkbox.checked ? 'en' : 'dis') + 'ableDataChannelRecordings');
  }
}

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Provides the UI for dump creation.
 */
var DumpCreator = (function() {
  /**
   * @param {Element} containerElement The parent element of the dump creation
   *     UI.
   * @constructor
   */
  function DumpCreator(containerElement) {
    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    this.root_ = document.createElement('details');

    this.root_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.root_);
    var summary = document.createElement('summary');
    this.root_.appendChild(summary);
    summary.textContent = 'Create Dump';
    var content = document.createElement('div');
    this.root_.appendChild(content);

    content.appendChild($('dump-template').content.cloneNode(true));
    content.getElementsByTagName('a')[0].addEventListener(
        'click', this.onDownloadData_.bind(this));
    content.getElementsByTagName('input')[0].addEventListener(
        'click', this.onAudioDebugRecordingsChanged_.bind(this));
    content.getElementsByTagName('input')[1].addEventListener(
        'click', this.onEventLogRecordingsChanged_.bind(this));
  }

  DumpCreator.prototype = {
    // Mark the diagnostic audio recording checkbox checked.
    setAudioDebugRecordingsCheckbox: function() {
      this.root_.getElementsByTagName('input')[0].checked = true;
    },

    // Mark the diagnostic audio recording checkbox unchecked.
    clearAudioDebugRecordingsCheckbox: function() {
      this.root_.getElementsByTagName('input')[0].checked = false;
    },

    // Mark the event log recording checkbox checked.
    setEventLogRecordingsCheckbox: function() {
      this.root_.getElementsByTagName('input')[1].checked = true;
    },

    // Mark the event log recording checkbox unchecked.
    clearEventLogRecordingsCheckbox: function() {
      this.root_.getElementsByTagName('input')[1].checked = false;
    },

    // Mark the event log recording checkbox as mutable/immutable.
    setEventLogRecordingsCheckboxMutability: function(mutable) {
      // TODO(eladalon): Remove reliance on number and order of elements.
      // https://crbug.com/817391
      this.root_.getElementsByTagName('input')[1].disabled = !mutable;
      if (!mutable) {
        var label = this.root_.getElementsByTagName('label')[2];
        label.style = 'color:red;';
        label.textContent =
            ' WebRTC event logging\'s state was set by a command line flag.';
      }
    },

    /**
     * Downloads the PeerConnection updates and stats data as a file.
     *
     * @private
     */
    onDownloadData_: function() {
      var dumpObject = {
        'getUserMedia': userMediaRequests,
        'PeerConnections': peerConnectionDataStore,
        'UserAgent': navigator.userAgent,
      };
      var textBlob = new Blob(
          [JSON.stringify(dumpObject, null, 1)], {type: 'octet/stream'});
      var URL = window.URL.createObjectURL(textBlob);

      var anchor = this.root_.getElementsByTagName('a')[0];
      anchor.href = URL;
      anchor.download = 'webrtc_internals_dump.txt';
      // The default action of the anchor will download the URL.
    },

    /**
     * Handles the event of toggling the audio debug recordings state.
     *
     * @private
     */
    onAudioDebugRecordingsChanged_: function() {
      var enabled = this.root_.getElementsByTagName('input')[0].checked;
      if (enabled) {
        chrome.send('enableAudioDebugRecordings');
      } else {
        chrome.send('disableAudioDebugRecordings');
      }
    },

    /**
     * Handles the event of toggling the event log recordings state.
     *
     * @private
     */
    onEventLogRecordingsChanged_: function() {
      var enabled = this.root_.getElementsByTagName('input')[1].checked;
      if (enabled) {
        chrome.send('enableEventLogRecordings');
      } else {
        chrome.send('disableEventLogRecordings');
      }
    },
  };
  return DumpCreator;
})();

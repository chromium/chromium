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

    content.innerHTML = '<div><a><button>' +
        'Download the PeerConnection updates and stats data' +
        '</button></a></div>' +
        '<p><label><input type=checkbox>' +
        'Enable diagnostic audio recordings</label></p>' +
        '<p class=audio-diagnostic-dumps-info>A diagnostic audio recording is' +
        ' used for analyzing audio problems. It consists of several files and' +
        ' contains the audio played out to the speaker (output) and captured' +
        ' from the microphone (input). The data is saved locally.' +
        ' Checking this box will enable recordings of all ongoing input and' +
        ' output audio streams (including non-WebRTC streams) and for future' +
        ' audio streams. When the box is unchecked or this page is closed,' +
        ' all ongoing recordings will be stopped and this recording' +
        ' functionality disabled. Recording audio from multiple tabs is' +
        ' supported as well as multiple recordings from the same tab.</p>' +
        '<p>When enabling, select a base filename to which the following' +
        ' suffixes will be added:</p>' +
        '<p><div>&lt;base filename&gt;.&lt;render process ID&gt;' +
        '.aec_dump.&lt;AEC dump recording ID&gt;</div>' +
        '<div>&lt;base filename&gt;.input.&lt;stream recording ID&gt;.wav' +
        '</div><div>' +
        '&lt;base filename&gt;.output.&lt;stream recording ID&gt;.wav' +
        '</div></p>' +
        '<p class=audio-diagnostic-dumps-info>It is recommended to choose a' +
        ' new base filename each time the feature is enabled to avoid ending' +
        ' up with partially overwritten or unusable audio files.</p>' +
        '<p><label><input type=checkbox disabled=true>' +
        'Enable diagnostic packet and event recording' +
        '<label name="placeholder_for_warning"/></label></p>' +
        '<p class=audio-diagnostic-dumps-info>A diagnostic packet and event' +
        ' recording can be used for analyzing various issues related to' +
        ' thread starvation, jitter buffers or bandwidth estimation. Two' +
        ' types of data are logged. First, incoming and outgoing RTP headers' +
        ' and RTCP packets are logged. These do not include any audio or' +
        ' video information, nor any other types of personally identifiable' +
        ' information (so no IP addresses or URLs). Checking this box will' +
        ' enable the recording for ongoing WebRTC calls and for future' +
        ' WebRTC calls. When the box is unchecked or this page is closed,' +
        ' all ongoing recordings will be stopped and this recording' +
        ' functionality will be disabled for future WebRTC calls. Recording' +
        ' in multiple tabs or multiple recordings in the same tab will cause' +
        ' multiple log files to be created. When enabling, a filename for the' +
        ' recording can be entered. The entered filename is used as a' +
        ' base, to which the following suffixes will be appended.</p>' +
        ' <p>&lt;base filename&gt;_&lt;date&gt;_&lt;timestamp&gt;_&lt;render ' +
        'process ID&gt;_&lt;recording ID&gt;</p>' +
        '<p class=audio-diagnostic-dumps-info>If a file with the same name' +
        ' already exists, it will be overwritten. No more than 5 logfiles ' +
        ' will be created, and each of them is limited to 60MB of storage. ' +
        ' On Android these limits are 3 files of at most 10MB each. ' +
        ' When the limit is reached, the checkbox must be unchecked and ' +
        ' rechecked to resume logging.</p>';
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
      var dump_object = {
        'getUserMedia': userMediaRequests,
        'PeerConnections': peerConnectionDataStore,
        'UserAgent': navigator.userAgent,
      };
      var textBlob = new Blob(
          [JSON.stringify(dump_object, null, ' ')], {type: 'octet/stream'});
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

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * The data of a peer connection update.
 * @param {number} pid The id of the renderer.
 * @param {number} lid The id of the peer conneciton inside a renderer.
 * @param {string} type The type of the update.
 * @param {string} value The details of the update.
 * @constructor
 */
var PeerConnectionUpdateEntry = function(pid, lid, type, value) {
  /**
   * @type {number}
   */
  this.pid = pid;

  /**
   * @type {number}
   */
  this.lid = lid;

  /**
   * @type {string}
   */
  this.type = type;

  /**
   * @type {string}
   */
  this.value = value;
};


/**
 * Maintains the peer connection update log table.
 */
var PeerConnectionUpdateTable = (function() {
  'use strict';

  /**
   * @constructor
   */
  function PeerConnectionUpdateTable() {
    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_ID_SUFFIX_ = '-update-log';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_CONTAINER_CLASS_ = 'update-log-container';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_TABLE_CLASS = 'update-log-table';
  }

  PeerConnectionUpdateTable.prototype = {
    /**
     * Adds the update to the update table as a new row. The type of the update
     * is set to the summary of the cell; clicking the cell will reveal or hide
     * the details as the content of a TextArea element.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @param {!PeerConnectionUpdateEntry} update The update to add.
     */
    addPeerConnectionUpdate: function(peerConnectionElement, update) {
      var tableElement = this.ensureUpdateContainer_(peerConnectionElement);

      var row = document.createElement('tr');
      tableElement.firstChild.appendChild(row);

      var time = new Date(parseFloat(update.time));
      row.innerHTML = '<td>' + time.toLocaleString() + '</td>';

      // map internal event names to spec event names.
      var type = {
        onRenegotiationNeeded: 'negotiationneeded',
        signalingStateChange: 'signalingstatechange',
        iceGatheringStateChange: 'icegatheringstatechange',
        legacyIceConnectionStateChange: 'iceconnectionstatechange (legacy)',
        iceConnectionStateChange: 'iceconnectionstatechange',
        connectionStateChange: 'connectionstatechange',
        onIceCandidate: 'icecandidate',
        stop: 'close'
      }[update.type] ||
          update.type;

      if (update.value.length == 0) {
        row.innerHTML += '<td>' + type + '</td>';
        return;
      }

      if (update.type === 'onIceCandidate' ||
          update.type === 'addIceCandidate') {
        // extract ICE candidate type from the field following typ.
        var candidateType = update.value.match(/(?: typ )(host|srflx|relay)/);
        if (candidateType) {
          type += ' (' + candidateType[1] + ')';
        }
      }
      row.innerHTML +=
          '<td><details><summary>' + type + '</summary></details></td>';

      var valueContainer = document.createElement('pre');
      var details = row.cells[1].childNodes[0];
      details.appendChild(valueContainer);

      // Highlight ICE failures and failure callbacks.
      if ((update.type === 'iceConnectionStateChange' &&
           update.value === 'ICEConnectionStateFailed') ||
          update.type.indexOf('OnFailure') !== -1 ||
          update.type === 'addIceCandidateFailed') {
        valueContainer.parentElement.classList.add('update-log-failure');
      }

      var value = update.value;
      // map internal names and values to names and events from the
      // specification. This is a display change which shall not
      // change the JSON dump.
      if (update.type === 'iceConnectionStateChange') {
        value = {
          ICEConnectionStateNew: 'new',
          ICEConnectionStateChecking: 'checking',
          ICEConnectionStateConnected: 'connected',
          ICEConnectionStateCompleted: 'completed',
          ICEConnectionStateFailed: 'failed',
          ICEConnectionStateDisconnected: 'disconnected',
          ICEConnectionStateClosed: 'closed',
        }[value] ||
            value;
      } else if (update.type === 'iceGatheringStateChange') {
        value = {
          ICEGatheringStateNew: 'new',
          ICEGatheringStateGathering: 'gathering',
          ICEGatheringStateComplete: 'complete',
        }[value] ||
            value;
      } else if (update.type === 'signalingStateChange') {
        value = {
          SignalingStateStable: 'stable',
          SignalingStateHaveLocalOffer: 'have-local-offer',
          SignalingStateHaveRemoteOffer: 'have-remote-offer',
          SignalingStateHaveLocalPrAnswer: 'have-local-pranswer',
          SignalingStateHaveRemotePrAnswer: 'have-remote-pranswer',
          SignalingStateClosed: 'closed',
        }[value] ||
            value;
      }

      valueContainer.textContent = value;
    },

    /**
     * Makes sure the update log table of the peer connection is created.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @return {!Element} The log table element.
     * @private
     */
    ensureUpdateContainer_: function(peerConnectionElement) {
      var tableId = peerConnectionElement.id + this.UPDATE_LOG_ID_SUFFIX_;
      var tableElement = $(tableId);
      if (!tableElement) {
        var tableContainer = document.createElement('div');
        tableContainer.className = this.UPDATE_LOG_CONTAINER_CLASS_;
        peerConnectionElement.appendChild(tableContainer);

        tableElement = document.createElement('table');
        tableElement.className = this.UPDATE_LOG_TABLE_CLASS;
        tableElement.id = tableId;
        tableElement.border = 1;
        tableContainer.appendChild(tableElement);
        tableElement.innerHTML = '<tr><th>Time</th>' +
            '<th class="update-log-header-event">Event</th></tr>';
      }
      return tableElement;
    }
  };

  return PeerConnectionUpdateTable;
})();

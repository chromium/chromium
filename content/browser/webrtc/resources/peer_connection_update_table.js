// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';
import * as SDPUtils from './sdp_utils.js';

const MAX_NUMBER_OF_STATE_CHANGES_DISPLAYED = 10;
const MAX_NUMBER_OF_EXPANDED_MEDIASECTIONS = 10;
/**
 * The data of a peer connection update.
 * @param {number} pid The id of the renderer.
 * @param {number} lid The id of the peer conneciton inside a renderer.
 * @param {string} type The type of the update.
 * @param {string} value The details of the update.
 */
class PeerConnectionUpdateEntry {
  constructor(pid, lid, type, value) {
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
  }
}

/**
 * Maintains the peer connection update log table.
 */
export class PeerConnectionUpdateTable {
  constructor() {
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

  /**
   * Adds the update to the update table as a new row. The type of the update
   * is set to the summary of the cell; clicking the cell will reveal or hide
   * the details as the content of a TextArea element.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @param {!PeerConnectionUpdateEntry} update The update to add.
   */
  addPeerConnectionUpdate(peerConnectionElement, update) {
    const tableElement = this.ensureUpdateContainer_(peerConnectionElement);

    const row = document.createElement('tr');
    tableElement.firstChild.appendChild(row);

    const time = new Date(parseFloat(update.time));
    const timeItem = document.createElement('td');
    timeItem.textContent = time.toLocaleString();
    row.appendChild(timeItem);

    // `type` is a display variant of update.type which does not get serialized
    // into the JSON dump.
    let type = update.type;

    if (update.value.length === 0) {
      const typeItem = document.createElement('td');
      typeItem.textContent = type;
      row.appendChild(typeItem);
      return;
    }

    if (update.type === 'icecandidate' || update.type === 'addIceCandidate') {
      const parts = update.value.split(', ');
      type += '(' + parts[0] + ', ' + parts[1]; // show sdpMid/sdpMLineIndex.
      const candidateParts = parts[2].substring(11).split(' ');
      if (candidateParts && candidateParts[7]) { // show candidate type.
        type += ', type: ' + candidateParts[7];
      }
      if (parts[3]) { // url, if present.
        type += ', ' + parts[3];
      }
      if (parts[4]) { // relayProtocol, if present.
        type += ', ' + parts[4];
      }
      type += ')';
    } else if (
        update.type === 'createOfferOnSuccess' ||
        update.type === 'createAnswerOnSuccess') {
      this.setLastOfferAnswer_(tableElement, update);
    } else if (update.type === 'setLocalDescription') {
      const lastOfferAnswer = this.getLastOfferAnswer_(tableElement);
      if (update.value.startsWith('type: rollback')) {
        this.setLastOfferAnswer_(tableElement, {value: undefined})
      } else if (lastOfferAnswer && update.value !== lastOfferAnswer) {
        type += ' (munged)';
      }
    } else if (update.type === 'setConfiguration') {
      // Update the configuration that is displayed at the top.
      peerConnectionElement.firstChild.children[2].textContent = update.value;
    } else if (['transceiverAdded',
        'transceiverModified'].includes(update.type)) {
      // Show the transceiver index.
      const indexLine = update.value.split('\n', 3)[2];
      if (indexLine.startsWith('getTransceivers()[')) {
        type += ' ' + indexLine.substring(17, indexLine.length - 2);
      }
      const kindLine = update.value.split('\n', 5)[4].trim();
      if (kindLine.startsWith('kind:')) {
        type += ', ' + kindLine.substring(6, kindLine.length - 2);
      }
    } else if (['iceconnectionstatechange', 'connectionstatechange',
        'signalingstatechange'].includes(update.type)) {
      const fieldName = {
        'iceconnectionstatechange' : 'iceconnectionstate',
        'connectionstatechange' : 'connectionstate',
        'signalingstatechange' : 'signalingstate',
      }[update.type];
      const el = peerConnectionElement.getElementsByClassName(fieldName)[0];
      const numberOfEvents = el.textContent.split(' => ').length;
      if (numberOfEvents < MAX_NUMBER_OF_STATE_CHANGES_DISPLAYED) {
        el.textContent += ' => ' + update.value;
      } else if (numberOfEvents >= MAX_NUMBER_OF_STATE_CHANGES_DISPLAYED) {
        el.textContent += ' => ...';
      }
    }

    const summaryItem = $('summary-template').content.cloneNode(true);
    const summary = summaryItem.querySelector('summary');
    summary.textContent = type;
    row.appendChild(summaryItem);

    const valueContainer = document.createElement('pre');
    const details = row.cells[1].childNodes[0];
    details.appendChild(valueContainer);

    // Highlight ICE/DTLS failures and failure callbacks.
    if ((update.type === 'iceconnectionstatechange' &&
         update.value === 'failed') ||
        (update.type === 'connectionstatechange' &&
         update.value === 'failed') ||
        update.type.indexOf('OnFailure') !== -1 ||
        update.type === 'addIceCandidateFailed') {
      valueContainer.parentElement.classList.add('update-log-failure');
    }

    // RTCSessionDescription is serialized as 'type: <type>, sdp:'
    if (update.value.indexOf(', sdp:') !== -1) {
      const [type, sdp] = update.value.substring(6).split(', sdp: ');
      if (type === 'rollback') {
        // Rollback has no SDP.
        summary.textContent += ' (type: "rollback")';
      } else {
        // Create a copy-to-clipboard button.
        const copyBtn = document.createElement('button');
        copyBtn.textContent = 'Copy description to clipboard';
        copyBtn.onclick = () => {
          navigator.clipboard.writeText(JSON.stringify({type, sdp}));
        };
        valueContainer.appendChild(copyBtn);

        let lastSections;
        const lastOfferAnswer = this.getLastOfferAnswer_(tableElement);
        if (lastOfferAnswer && lastOfferAnswer !== update.value) {
          lastSections = SDPUtils.splitSections(
              lastOfferAnswer.substring(6).split(', sdp: ')[1]);
        }
        // Fold the SDP sections.
        const sections = SDPUtils.splitSections(sdp);
        summary.textContent +=
          ' (type: "' + type + '", ' + sections.length + ' sections)';
        sections.forEach((section, index) => {
          const lines = SDPUtils.splitLines(section);
          // Extract the mid attribute.
          const mid = lines
              .filter(line => line.startsWith('a=mid:'))
              .map(line => line.substring(6))[0];
          const direction = SDPUtils.getDirection(section, sections[0]);

          const sectionDetails = document.createElement('details');
          const rejected = index !== 0 &&
              SDPUtils.parseMLine(lines[0]).port === 0;
          // Fold by default for large SDP, inactive SDP or rejected m-lines.
          sectionDetails.open =
            sections.length <= MAX_NUMBER_OF_EXPANDED_MEDIASECTIONS &&
            direction !== 'inactive' && !rejected;
          sectionDetails.textContent = lines.slice(1).join('\n');

          const sectionSummary = document.createElement('summary');
          sectionSummary.textContent =
            lines[0].trim() +
            ' (' + (lines.length - 1) + ' more lines)' +
            (section.startsWith('m=') ? ' direction=' + direction : '') +
            (mid ? ' mid=' + mid : '');
          if (lastSections && lastSections[index] !== sections[index]) {
            // Open munged sections by default and give visual feedback.
            sectionDetails.open = true;
            sectionSummary.textContent += ' munged';
            sectionSummary.style.backgroundColor = '#FBCEB1';
            const lastLines = SDPUtils.splitLines(lastSections[index]);
            // Show the first different line using a HTML title 'popover'.
            for (let i = 0; i < lines.length && i < lastLines.length; i++) {
              if (lines[i] !== lastLines[i]) {
                sectionSummary.title = 'First different line: ' + lines[i];
                break;
              }
            }
          }
          sectionDetails.appendChild(sectionSummary);

          valueContainer.appendChild(sectionDetails);
        });
      }
    } else if (update.type === 'icecandidate' ||
        update.type === 'addIceCandidate') {
      const parts = update.value.split(', ');
      valueContainer.textContent = parts.slice(0, 2).join(', ') + '\n' +
        parts[2] + '\n' +
        parts.slice(3).join(', ');
    } else {
      valueContainer.textContent = update.value;
    }
  }

  /**
   * Makes sure the update log table of the peer connection is created.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @return {!Element} The log table element.
   * @private
   */
  ensureUpdateContainer_(peerConnectionElement) {
    const tableId = peerConnectionElement.id + this.UPDATE_LOG_ID_SUFFIX_;

  // Disable getElementById restriction here, since |tableId| is not always
  // a valid selector.
  // eslint-disable-next-line no-restricted-properties
    let tableElement = document.getElementById(tableId);
    if (!tableElement) {
      const tableContainer = document.createElement('div');
      tableContainer.className = this.UPDATE_LOG_CONTAINER_CLASS_;
      peerConnectionElement.appendChild(tableContainer);

      tableElement = document.createElement('table');
      tableElement.className = this.UPDATE_LOG_TABLE_CLASS;
      tableElement.id = tableId;
      tableElement.border = 1;
      tableContainer.appendChild(tableElement);
      tableElement.appendChild(
          $('time-event-template').content.cloneNode(true));
    }
    return tableElement;
  }

  /**
   * Store the last createOfferOnSuccess/createAnswerOnSuccess to compare to
   * setLocalDescription and visualize SDP munging.
   *
   * @param {!Element} tableElement The peerconnection update element.
   * @param {!PeerConnectionUpdateEntry} update The update to add.
   * @private
   */
  setLastOfferAnswer_(tableElement, update) {
    tableElement['data-lastofferanswer'] = update.value;
  }

  /**
   * Retrieves the last createOfferOnSuccess/createAnswerOnSuccess to compare
   * to setLocalDescription and visualize SDP munging.
   *
   * @param {!Element} tableElement The peerconnection update element.
   * @private
   */
  getLastOfferAnswer_(tableElement) {
    return tableElement['data-lastofferanswer'];
  }
}

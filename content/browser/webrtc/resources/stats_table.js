// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {GetSsrcFromReport, SsrcInfoManager} from './ssrc_info_manager.js';

/**
 * Maintains the stats table.
 * @param {SsrcInfoManager} ssrcInfoManager The source of the ssrc info.
 */
export class StatsTable {
  /**
   * @param {SsrcInfoManager} ssrcInfoManager The source of the ssrc info.
   */
  constructor(ssrcInfoManager) {
    /**
     * @type {SsrcInfoManager}
     * @private
     */
    this.ssrcInfoManager_ = ssrcInfoManager;
  }

  /**
   * Adds |report| to the stats table of |peerConnectionElement|.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @param {!Object} report The object containing stats, which is the object
   *     containing timestamp and values, which is an array of strings, whose
   *     even index entry is the name of the stat, and the odd index entry is
   *     the value.
   */
  addStatsReport(peerConnectionElement, report) {
    if (report.type === 'codec') {
      return;
    }
    const statsTable = this.ensureStatsTable_(peerConnectionElement, report);

    if (['outbound-rtp', 'inbound-rtp'].includes(report.type)
        && report.stats.values) {
      let summary = report.id + ' (' + report.type;
      // Show mid, rid and codec for inbound-rtp and outbound-rtp.
      // Note: values is an array [key1, val1, key2, val2, ...] so searching
      // for a certain key needs to ensure it does not collide with a value.
      const midIndex = report.stats.values.findIndex((value, index) => {
        return value === 'mid' && index % 2 === 0;
      });
      if (midIndex !== -1) {
        const midInfo = report.stats.values[midIndex + 1];
        summary += ', mid=' + midInfo;
      }
      const ridIndex = report.stats.values.findIndex((value, index) => {
        return value === 'rid' && index % 2 === 0;
      });
      if (ridIndex !== -1) {
        const ridInfo = report.stats.values[ridIndex + 1];
        summary += ', rid=' + ridInfo;
      }

      const codecIndex = report.stats.values.findIndex((value, index) => {
        return value === '[codec]' && index % 2 === 0;
      });
      if (codecIndex !== -1) {
        const codecInfo = report.stats.values[codecIndex + 1].split(' ')[0];
        summary += ', ' + codecInfo;
      }
      // Update the summary.
      statsTable.parentElement.firstElementChild.innerText = summary + ')';
    }

    if (report.stats) {
      this.addStatsToTable_(
          statsTable, report.stats.timestamp, report.stats.values);
    }
  }

  clearStatsLists(peerConnectionElement) {
    const containerId = peerConnectionElement.id + '-table-container';
    const container = $(containerId);
    if (container) {
      peerConnectionElement.removeChild(container);
      this.ensureStatsTableContainer_(peerConnectionElement);
    }
  }

  /**
   * Ensure the DIV container for the stats tables is created as a child of
   * |peerConnectionElement|.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @return {!Element} The stats table container.
   * @private
   */
  ensureStatsTableContainer_(peerConnectionElement) {
    const containerId = peerConnectionElement.id + '-table-container';
    let container = $(containerId);
    if (!container) {
      container = document.createElement('div');
      container.id = containerId;
      container.className = 'stats-table-container';
      const head = document.createElement('div');
      head.textContent = 'Stats Tables';
      container.appendChild(head);
      peerConnectionElement.appendChild(container);
    }
    return container;
  }

  /**
   * Ensure the stats table for track specified by |report| of PeerConnection
   * |peerConnectionElement| is created.
   *
   * @param {!Element} peerConnectionElement The root element.
   * @param {!Object} report The object containing stats, which is the object
   *     containing timestamp and values, which is an array of strings, whose
   *     even index entry is the name of the stat, and the odd index entry is
   *     the value.
   * @return {!Element} The stats table element.
   * @private
   */
  ensureStatsTable_(peerConnectionElement, report) {
    const tableId = peerConnectionElement.id + '-table-' + report.id;
    let table = $(tableId);
    if (!table) {
      const container = this.ensureStatsTableContainer_(peerConnectionElement);
      const details = document.createElement('details');
      container.appendChild(details);

      const summary = document.createElement('summary');
      summary.textContent = report.id + ' (' + report.type + ')';
      details.appendChild(summary);

      table = document.createElement('table');
      details.appendChild(table);
      table.id = tableId;
      table.border = 1;

      table.appendChild($('trth-template').content.cloneNode(true));
      table.rows[0].cells[0].textContent = 'Statistics ' + report.id;
      if (report.type === 'ssrc') {
        table.insertRow(1);
        table.rows[1].appendChild(
            $('td-colspan-template').content.cloneNode(true));
        this.ssrcInfoManager_.populateSsrcInfo(
            table.rows[1].cells[0], GetSsrcFromReport(report));
      }
    }
    return table;
  }

  /**
   * Update |statsTable| with |time| and |statsData|.
   *
   * @param {!Element} statsTable Which table to update.
   * @param {number} time The number of milliseconds since epoch.
   * @param {Array<string>} statsData An array of stats name and value pairs.
   * @private
   */
  addStatsToTable_(statsTable, time, statsData) {
    const date = new Date(time);
    this.updateStatsTableRow_(statsTable, 'timestamp', date.toLocaleString());
    for (let i = 0; i < statsData.length - 1; i = i + 2) {
      this.updateStatsTableRow_(statsTable, statsData[i], statsData[i + 1]);
    }
  }

  /**
   * Update the value column of the stats row of |rowName| to |value|.
   * A new row is created is this is the first report of this stats.
   *
   * @param {!Element} statsTable Which table to update.
   * @param {string} rowName The name of the row to update.
   * @param {string} value The new value to set.
   * @private
   */
  updateStatsTableRow_(statsTable, rowName, value) {
    const trId = statsTable.id + '-' + rowName;
    let trElement = $(trId);
    const activeConnectionClass = 'stats-table-active-connection';
    if (!trElement) {
      trElement = document.createElement('tr');
      trElement.id = trId;
      statsTable.firstChild.appendChild(trElement);
      const item = $('td2-template').content.cloneNode(true);
      item.querySelector('td').textContent = rowName;
      trElement.appendChild(item);
    }
    trElement.cells[1].textContent = value;

    // Highlights the table for the active connection.
    if (rowName === 'googActiveConnection') {
      if (value === true) {
        statsTable.parentElement.classList.add(activeConnectionClass);
      } else {
        statsTable.parentElement.classList.remove(activeConnectionClass);
      }
    }
  }
}

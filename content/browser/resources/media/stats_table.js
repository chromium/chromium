// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Maintains the stats table.
 * @param {SsrcInfoManager} ssrcInfoManager The source of the ssrc info.
 */
var StatsTable = (function(ssrcInfoManager) {
  'use strict';

  /**
   * @param {SsrcInfoManager} ssrcInfoManager The source of the ssrc info.
   * @constructor
   */
  function StatsTable(ssrcInfoManager) {
    /**
     * @type {SsrcInfoManager}
     * @private
     */
    this.ssrcInfoManager_ = ssrcInfoManager;
  }

  StatsTable.prototype = {
    /**
     * Adds |report| to the stats table of |peerConnectionElement|.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @param {!Object} report The object containing stats, which is the object
     *     containing timestamp and values, which is an array of strings, whose
     *     even index entry is the name of the stat, and the odd index entry is
     *     the value.
     */
    addStatsReport: function(peerConnectionElement, report) {
      if (report.type == 'codec') {
        return;
      }
      var statsTable = this.ensureStatsTable_(peerConnectionElement, report);

      if (report.stats) {
        this.addStatsToTable_(
            statsTable, report.stats.timestamp, report.stats.values);
      }
    },

    clearStatsLists: function(peerConnectionElement) {
      let containerId = peerConnectionElement.id + '-table-container';
      let container = $(containerId);
      if (container) {
        peerConnectionElement.removeChild(container);
        this.ensureStatsTableContainer_(peerConnectionElement);
      }
    },

    /**
     * Ensure the DIV container for the stats tables is created as a child of
     * |peerConnectionElement|.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @return {!Element} The stats table container.
     * @private
     */
    ensureStatsTableContainer_: function(peerConnectionElement) {
      var containerId = peerConnectionElement.id + '-table-container';
      var container = $(containerId);
      if (!container) {
        container = document.createElement('div');
        container.id = containerId;
        container.className = 'stats-table-container';
        var head = document.createElement('div');
        head.textContent = 'Stats Tables';
        container.appendChild(head);
        peerConnectionElement.appendChild(container);
      }
      return container;
    },

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
    ensureStatsTable_: function(peerConnectionElement, report) {
      var tableId = peerConnectionElement.id + '-table-' + report.id;
      var table = $(tableId);
      if (!table) {
        var container = this.ensureStatsTableContainer_(peerConnectionElement);
        var details = document.createElement('details');
        container.appendChild(details);

        var summary = document.createElement('summary');
        summary.textContent = report.id + ' (' + report.type + ')';
        details.appendChild(summary);

        table = document.createElement('table');
        details.appendChild(table);
        table.id = tableId;
        table.border = 1;

        table.innerHTML = '<tr><th colspan=2></th></tr>';
        table.rows[0].cells[0].textContent = 'Statistics ' + report.id;
        if (report.type == 'ssrc') {
          table.insertRow(1);
          table.rows[1].innerHTML = '<td colspan=2></td>';
          this.ssrcInfoManager_.populateSsrcInfo(
              table.rows[1].cells[0], GetSsrcFromReport(report));
        }
      }
      return table;
    },

    /**
     * Update |statsTable| with |time| and |statsData|.
     *
     * @param {!Element} statsTable Which table to update.
     * @param {number} time The number of miliseconds since epoch.
     * @param {Array<string>} statsData An array of stats name and value pairs.
     * @private
     */
    addStatsToTable_: function(statsTable, time, statsData) {
      var date = new Date(time);
      this.updateStatsTableRow_(statsTable, 'timestamp', date.toLocaleString());
      for (var i = 0; i < statsData.length - 1; i = i + 2) {
        this.updateStatsTableRow_(statsTable, statsData[i], statsData[i + 1]);
      }
    },

    /**
     * Update the value column of the stats row of |rowName| to |value|.
     * A new row is created is this is the first report of this stats.
     *
     * @param {!Element} statsTable Which table to update.
     * @param {string} rowName The name of the row to update.
     * @param {string} value The new value to set.
     * @private
     */
    updateStatsTableRow_: function(statsTable, rowName, value) {
      var trId = statsTable.id + '-' + rowName;
      var trElement = $(trId);
      var activeConnectionClass = 'stats-table-active-connection';
      if (!trElement) {
        trElement = document.createElement('tr');
        trElement.id = trId;
        statsTable.firstChild.appendChild(trElement);
        trElement.innerHTML = '<td>' + rowName + '</td><td></td>';
      }
      trElement.cells[1].textContent = value;

      // Highlights the table for the active connection.
      if (rowName == 'googActiveConnection') {
        if (value === true) {
          statsTable.parentElement.classList.add(activeConnectionClass);
        } else {
          statsTable.parentElement.classList.remove(activeConnectionClass);
        }
      }
    }
  };

  return StatsTable;
})();

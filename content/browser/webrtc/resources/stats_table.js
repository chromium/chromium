// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';

import {generateStatsLabel} from './stats_helper.js';

/**
 * Maintains the stats table.
 */
export class StatsTable {
  constructor() {}

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
    const statsTable = this.ensureStatsTable_(peerConnectionElement, report);

    // Update the label since information may have changed.
    statsTable.parentElement.firstElementChild.innerText =
        generateStatsLabel(report);

    if (report.stats) {
      this.addStatsToTable_(
          statsTable, report.stats.timestamp, report.stats.values);
    }
  }

  clearStatsLists(peerConnectionElement) {
    const containerId = peerConnectionElement.id + '-table-container';
    // Disable getElementById restriction here, since |containerId| is not
    // always a valid selector.
    // eslint-disable-next-line no-restricted-properties
    const container = document.getElementById(containerId);
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
    // Disable getElementById restriction here, since |containerId| is not
    // always a valid selector.
    // eslint-disable-next-line no-restricted-properties
    let container = document.getElementById(containerId);
    if (!container) {
      container = document.createElement('div');
      container.id = containerId;
      container.className = 'stats-table-container';
      const head = document.createElement('div');
      head.textContent = 'Stats Tables';
      container.appendChild(head);
      const label = document.createElement('label');
      label.innerText = 'Filter statistics by type including ';
      container.appendChild(label);
      const input = document.createElement('input');
      input.placeholder = 'separate multiple values by `,`';
      input.size = 25;
      input.oninput = (e) => this.filterStats(e, container);
      container.appendChild(input);
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
    // Disable getElementById restriction here, since |tableId| is not
    // always a valid selector.
    // eslint-disable-next-line no-restricted-properties
    let table = document.getElementById(tableId);
    if (!table) {
      const container = this.ensureStatsTableContainer_(peerConnectionElement);
      const details = document.createElement('details');
      details.attributes['data-statsType'] = report.type;
      container.appendChild(details);

      const summary = document.createElement('summary');
      summary.textContent = generateStatsLabel(report);
      details.appendChild(summary);

      table = document.createElement('table');
      details.appendChild(table);
      table.id = tableId;
      table.border = 1;

      table.appendChild($('trth-template').content.cloneNode(true));
      table.rows[0].cells[0].textContent = 'Statistics ' + report.id;
      table['data-peerconnection-id'] = peerConnectionElement.id;
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
    const definedMetrics = new Set();
    for (let i = 0; i < statsData.length - 1; i = i + 2) {
      definedMetrics.add(statsData[i]);
    }
    // For any previously reported metric that is no longer defined, replace its
    // now obsolete value with the magic string "(removed)".
    const metricsContainer = statsTable.firstChild;
    for (let i = 0; i < metricsContainer.children.length; ++i) {
      const metricElement = metricsContainer.children[i];
      // `metricElement` IDs have the format `bla-bla-bla-bla-${metricName}`.
      let metricName =
          metricElement.id.substring(metricElement.id.lastIndexOf('-') + 1);
      if (metricName.endsWith(']')) {
        // Computed metrics may contain the '-' character (e.g.
        // `DifferenceCalculator` based metrics) in which case `metricName` will
        // not have been parsed correctly. Instead look for starting '['.
        metricName =
            metricElement.id.substring(metricElement.id.indexOf('['));
      }
      if (metricName && metricName != 'timestamp' &&
          !definedMetrics.has(metricName)) {
        this.updateStatsTableRow_(statsTable, metricName, '(removed)');
      }
    }
    // Add or update all "metric: value" that have a defined value.
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
    // Disable getElementById restriction here, since |trId| is not always
    // a valid selector.
    // eslint-disable-next-line no-restricted-properties
    let trElement = document.getElementById(trId);
    const activeConnectionClass = 'stats-table-active-connection';
    if (!trElement) {
      trElement = document.createElement('tr');
      trElement.id = trId;
      statsTable.firstChild.appendChild(trElement);
      const item = $('statsrow-template').content.cloneNode(true);
      item.querySelector('td').textContent = rowName;
      trElement.appendChild(item);
    }
    trElement.cells[1].textContent = value;
    if (rowName.endsWith('Id')) {
      // unicode link symbol
      trElement.cells[2].children[0].textContent = ' \u{1F517}';
      trElement.cells[2].children[0].href =
        '#' + statsTable['data-peerconnection-id'] + '-table-' + value;
    }
  }

  /**
   * Apply a filter to the stats table
   * @param event InputEvent from the filter input field.
   * @param container stats table container element.
   * @private
   */
  filterStats(event, container) {
    const filter = event.target.value;
    const filters = filter.split(',');
    container.childNodes.forEach(node => {
      if (node.nodeName !== 'DETAILS') {
        return;
      }
      const statsType = node.attributes['data-statsType'];
      if (!filter || filters.includes(statsType) ||
          filters.find(f => statsType.includes(f))) {
        node.style.display = 'block';
      } else {
        node.style.display = 'none';
      }
    });
  }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {ConversionInternalsHandler, ConversionInternalsHandlerRemote, WebUIImpression} from './conversion_internals.mojom-webui.js';

/**
 * Reference to the backend providing all the data.
 * @type {ConversionInternalsHandlerRemote}
 */
let pageHandler = null;

/**
 * All impressions held in storage at last update.
 * @type {!Array<!WebUIImpression>}
 */
let impressions = null;

/**
 * All impressions held in storage at last update.
 * @type {!Array<!WebUIImpression>}
 */
let reports = null;

/**
 * Remove all rows from the given table.
 * @param {!HTMLElement} table DOM element corresponding to table body.
 */
function clearTable(table) {
  table.innerHTML = trustedTypes.emptyHTML;
}

/**
 * Converts a mojo origin into a user-readable string, omitting default ports.
 * @param {Origin} origin Origin to convert
 * @return {string}
 */
function UrlToText(origin) {
  if (origin.host.length == 0) {
    return 'Null';
  }

  let result = origin.scheme + '://' + origin.host;

  if ((origin.scheme == 'https' && origin.port != '443') ||
      (origin.scheme == 'http' && origin.port != '80')) {
    result += ':' + origin.port;
  }
  return result;
}

/**
 * Creates a single row for the impression table.
 * @param {!WebIUIImpression} impression The info to create the row.
 * @return {!HTMLElement}
 */
function createImpressionRow(impression) {
  const template = $('impressionrow').cloneNode(true);
  const td = template.content.querySelectorAll('td');

  td[0].textContent = impression.impressionData;
  td[1].textContent = UrlToText(impression.impressionOrigin);
  td[2].textContent = UrlToText(impression.conversionDestination);
  td[3].textContent = UrlToText(impression.reportingOrigin);
  td[4].textContent = new Date(impression.impressionTime).toLocaleString();
  td[5].textContent = new Date(impression.expiryTime).toLocaleString();
  return document.importNode(template.content, true);
}

/**
 * Creates a single row for the impression table.
 * @param {!WebUIImpression} impression The info to create the row.
 * @return {!HTMLElement}
 */
function createReportRow(report) {
  const template = $('reportrow').cloneNode(true);
  const td = template.content.querySelectorAll('td');

  td[0].textContent = report.impressionData;
  td[1].textContent = report.conversionData;
  td[2].textContent = UrlToText(report.conversionOrigin);
  td[3].textContent = UrlToText(report.reportingOrigin);
  td[4].textContent = new Date(report.reportTime).toLocaleString();
  td[5].textContent = report.attributionCredit;
  return document.importNode(template.content, true);
}

/**
 * Regenerates the impression table from |impressions|.
 */
function renderImpressionTable() {
  const impressionTable = $('impression-table-body');
  clearTable(impressionTable);
  impressions.forEach(
      impression =>
          impressionTable.appendChild(createImpressionRow(impression)));

  // If there are no impressions, add an empty row to indicate the table is
  // purposefully empty.
  if (!impressions.length) {
    const template = $('impressionrow').cloneNode(true);
    const td = template.content.querySelectorAll('td');
    td[0].textContent = 'No active impressions.';
    impressionTable.appendChild(document.importNode(template.content, true));
  }
}

/**
 * Regenerates the report table from |reports|.
 */
function renderReportTable() {
  const reportTable = $('report-table-body');
  clearTable(reportTable);
  reports.forEach(report => reportTable.appendChild(createReportRow(report)));

  // If there are no reports, add an empty row to indicate the table is
  // purposefully empty.
  if (!reports.length) {
    const template = $('reportrow').cloneNode(true);
    const td = template.content.querySelectorAll('td');
    td[0].textContent = 'No pending reports.';
    reportTable.appendChild(document.importNode(template.content, true));
  }
}

/**
 * Fetch all active impressions and pending reports from the backend and
 * populate the tables. Also update measurement enabled status.
 */
function updatePageData() {
  // Get the feature status for ConversionMeasurement and populate it.
  pageHandler.isMeasurementEnabled().then((response) => {
    $('feature-status-content').innerText =
        response.enabled ? 'enabled' : 'disabled';
  });

  pageHandler.getActiveImpressions().then((response) => {
    impressions = response.impressions;
    renderImpressionTable();
  });

  pageHandler.getPendingReports().then((response) => {
    reports = response.reports;
    renderReportTable();
  });
}

/**
 * Deletes all data stored by the conversions backend, and refreshes
 * page data once this operation has finished.
 */
function clearStorage() {
  pageHandler.clearStorage().then(() => {
    updatePageData();
  });
}

/**
 * Sends all conversion reports, and updates the page once they are sent.
 * Disables the button while the reports are still being sent.
 */
function sendReports() {
  const button = $('send-reports');
  const previousText = $('send-reports').innerText;

  button.disabled = true;
  button.innerText = 'Sending...';
  pageHandler.sendPendingReports().then(() => {
    updatePageData();
    button.disabled = false;
    button.innerText = previousText;
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup the mojo interface.
  pageHandler = ConversionInternalsHandler.getRemote();

  $('refresh').addEventListener('click', updatePageData);
  $('clear-data').addEventListener('click', clearStorage);
  $('send-reports').addEventListener('click', sendReports);

  // Automatically refresh every 2 minutes.
  setInterval(updatePageData, 2 * 60 * 1000);
  updatePageData();
});

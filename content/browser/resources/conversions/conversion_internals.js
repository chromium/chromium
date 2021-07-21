// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {$} from 'chrome://resources/js/util.m.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {ConversionInternalsHandler, ConversionInternalsHandlerRemote, SentReportInfo, SourceType, WebUIConversionReport, WebUIImpression} from './conversion_internals.mojom-webui.js';

/**
 * Reference to the backend providing all the data.
 * @type {?ConversionInternalsHandlerRemote}
 */
let pageHandler = null;

/**
 * All sources held in storage at last update.
 * @type {!Array<!WebUIImpression>}
 */
let sources = [];

/**
 * All reports held in storage at last update.
 * @type {!Array<!WebUIConversionReport>}
 */
let reports = [];

/**
 * All sent reports at last update.
 * @type {!Array<!SentReportInfo>}
 */
let sentReports = [];

/**
 * This is used to create TrustedHTML.
 * @type {?TrustedTypePolicy}
 */
let staticHtmlPolicy = null;

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
 * Converts a mojo SourceType into a user-readable string.
 * @param {SourceType} sourceType Source type to convert
 * @return {string}
 */
function SourceTypeToText(sourceType) {
  switch (sourceType) {
    case SourceType.kNavigation:
      return 'Navigation';
    case SourceType.kEvent:
      return 'Event';
    default:
      return sourceType.toString();
  }
}

/**
 * Creates a single row for the source table.
 * @param {!WebUIImpression} source The info to create the row.
 * @return {!Node}
 */
function createSourceRow(source) {
  const template = $('source-row').cloneNode(true);
  const td = template.content.querySelectorAll('td');

  td[0].textContent = source.impressionData;
  td[1].textContent = UrlToText(source.impressionOrigin);
  td[2].textContent = UrlToText(source.conversionDestination);
  td[3].textContent = UrlToText(source.reportingOrigin);
  td[4].textContent = new Date(source.impressionTime).toLocaleString();
  td[5].textContent = new Date(source.expiryTime).toLocaleString();
  td[6].textContent = SourceTypeToText(source.sourceType);
  td[7].textContent = source.priority;
  td[8].textContent = source.dedupKeys.join();
  return document.importNode(template.content, true);
}

/**
 * Creates a single row for the report table.
 * @param {!WebUIConversionReport} report The info to create the row.
 * @return {!Node}
 */
function createReportRow(report) {
  const template = $('report-row').cloneNode(true);
  const td = template.content.querySelectorAll('td');

  td[0].textContent = report.impressionData;
  td[1].textContent = report.conversionData;
  td[2].textContent = UrlToText(report.conversionOrigin);
  td[3].textContent = UrlToText(report.reportingOrigin);
  td[4].textContent = new Date(report.reportTime).toLocaleString();
  td[5].textContent = SourceTypeToText(report.sourceType);
  return document.importNode(template.content, true);
}

/**
 * Creates a single row for the sent report table.
 * @param {!SentReportInfo} info The info to create the row.
 * @return {!Node}
 */
function createSentReportRow(info) {
  const template = $('sent-report-row').cloneNode(true);
  const td = template.content.querySelectorAll('td');

  td[0].textContent = info.reportUrl.url;
  td[1].textContent = info.reportBody;
  td[2].textContent = info.httpResponseCode;
  return document.importNode(template.content, true);
}

/**
 * Regenerates the source table from |sources|.
 */
function renderSourceTable() {
  const sourceTable = $('source-table-body');
  assert(sourceTable);
  clearTable(sourceTable);
  sources.forEach(source => sourceTable.appendChild(createSourceRow(source)));

  // If there are no sources, add an empty row to indicate the table is
  // purposefully empty.
  if (!sources.length) {
    const template = $('source-row').cloneNode(true);
    const td = template.content.querySelectorAll('td');
    td[0].textContent = 'No active sources.';
    sourceTable.appendChild(document.importNode(template.content, true));
  }
}

/**
 * Regenerates the report table from |reports|.
 */
function renderReportTable() {
  const reportTable = $('report-table-body');
  assert(reportTable);
  clearTable(reportTable);
  reports.forEach(report => reportTable.appendChild(createReportRow(report)));

  // If there are no reports, add an empty row to indicate the table is
  // purposefully empty.
  if (!reports.length) {
    const template = $('report-row').cloneNode(true);
    const td = template.content.querySelectorAll('td');
    td[0].textContent = 'No pending reports.';
    reportTable.appendChild(document.importNode(template.content, true));
  }
}

/**
 * Regenerates the sent report table from |sentReports|.
 */
function renderSentReportTable() {
  const sentReportTable = $('sent-report-table-body');
  assert(sentReportTable);
  clearTable(sentReportTable);
  sentReports.forEach(
      report => sentReportTable.appendChild(createSentReportRow(report)));

  // If there are no sent reports, add an empty row to indicate the table is
  // purposefully empty.
  if (!sentReports.length) {
    const template = $('sent-report-row').cloneNode(true);
    const td = template.content.querySelectorAll('td');
    td[0].textContent = 'No sent reports.';
    sentReportTable.appendChild(document.importNode(template.content, true));
  }
}

/**
 * Fetch all active sources, pending reports, and sent reports from the
 * backend and populate the tables. Also update measurement enabled status.
 */
function updatePageData() {
  // Get the feature status for ConversionMeasurement and populate it.
  pageHandler.isMeasurementEnabled().then((response) => {
    $('feature-status-content').innerText =
        response.enabled ? 'enabled' : 'disabled';
    $('feature-status-content').classList.toggle('disabled', !response.enabled);

    const htmlString = 'The #conversion-measurement-debug-mode flag is ' +
        '<strong>enabled</strong>, ' +
        'reports are sent immediately and never pending.';

    if (window.trustedTypes) {
      if (staticHtmlPolicy === null) {
        staticHtmlPolicy = trustedTypes.createPolicy(
            'cr-ui-tree-js-static', {createHTML: () => htmlString});
      }
      $('debug-mode-content').innerHTML = staticHtmlPolicy.createHTML('');
    } else {
      $('debug-mode-content').innerHTML = htmlString;
    }

    if (!response.debugMode) {
      $('debug-mode-content').innerText = '';
    }
  });

  pageHandler.getActiveImpressions().then((response) => {
    sources = response.impressions;
    renderSourceTable();
  });

  pageHandler.getPendingReports().then((response) => {
    reports = response.reports;
    renderReportTable();
  });

  pageHandler.getSentReports().then((response) => {
    sentReports = response.reports;
    renderSentReportTable();
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

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {$, getRequiredElement} from 'chrome://resources/js/util.m.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {ConversionInternalsHandler, ConversionInternalsHandlerRemote, SourceType, WebUIConversionReport, WebUIConversionReport_Status, WebUIImpression,} from './conversion_internals.mojom-webui.js';

/**
 * @template T
 * @param {!T} a
 * @param {!T} b
 * @return {number}
 */
function compareDefault(a, b) {
  if (a < b) {
    return -1;
  }
  if (a > b) {
    return 1;
  }
  return 0;
}

/**
 * @template T
 * @template V
 */
class Column {
  /**
   * @param {string} header
   * @param {function(!T): !V} getValue
   * @param {?function(!T, !T): number} compare
   */
  constructor(
      header, getValue,
      compare = (a, b) => compareDefault(getValue(a), getValue(b))) {
    this.header = header;

    /** @protected */
    this.getValue = getValue;

    this.compare = compare;
  }

  /**
   * @param {!Element} td
   * @param {!T} row
   */
  render(td, row) {
    td.innerText = this.getValue(row);
  }
}

/**
 * @template T
 * @extends {Column<T, Date>}
 */
class DateColumn extends Column {
  /**
   * @param {string} header
   * @param {function(!T): Date} getValue
   */
  constructor(header, getValue) {
    super(header, getValue);
  }

  /** @override */
  render(td, row) {
    td.innerText = this.getValue(row).toLocaleString();
  }
}

/**
 * @template T
 * @extends {Column<T, string>}
 */
class CodeColumn extends Column {
  /**
   * @param {string} header
   * @param {function(!T): string} getValue
   */
  constructor(header, getValue) {
    super(header, getValue, /*compare=*/ null);
  }

  /** @override */
  render(td, row) {
    const code = td.ownerDocument.createElement('code');
    code.innerText = this.getValue(row);

    const pre = td.ownerDocument.createElement('pre');
    pre.appendChild(code);

    td.appendChild(pre);
  }
}

/**
 * @template T
 */
class TableModel {
  constructor() {
    /** @type {!Array<Column<T, ?>>} */
    this.cols;

    /** @type {string} */
    this.emptyRowText;

    /** @type {!Array<!T>} */
    this.rows;
  }

  /**
   * @param {!Element} tr
   * @param {T} data
   */
  styleRow(tr, data) {}

  /**
   * @param {number} idx
   * @param {boolean} descending
   */
  sort(idx, descending) {
    const multiplier = descending ? -1 : 1;
    this.rows.sort((a, b) => this.cols[idx].compare(a, b) * multiplier);
  }
}

/**
 * Table abstracts the logic for rendering and sorting a table by dynamically
 * modifying a given <div>'s prototype and managing its children. The table's
 * columns are supplied by a TableModel supplied to the decorate function. Each
 * Column knows how to render the underlying value of the row type T, and
 * optionally sort rows of type T by that value.
 *
 * @template T
 * @extends {HTMLDivElement}
 */
class Table {
  constructor() {
    /** @private {!TableModel<T>} */
    this.model;

    /** @private {number} */
    this.sortIdx;

    /** @private {boolean} */
    this.sortDesc;

    /** @private {!Element} */
    this.tbody;
  }

  /**
   * @template T
   * @param {!Element} self
   * @param {!TableModel<T>} model
   */
  static decorate(self, model) {
    self.__proto__ = Table.prototype;
    self = /** @type {!Table} */ (self);

    self.model = model;
    self.sortIdx = -1;
    self.sortDesc = false;

    const tr = self.ownerDocument.createElement('tr');
    self.model.cols.forEach((col, idx) => {
      const th = self.ownerDocument.createElement('th');
      th.scope = 'col';
      th.innerText = col.header;

      if (col.compare) {
        th.role = 'button';
        Table.setSortAttrs(th, /*sortDesc=*/ null);
        th.addEventListener('click', () => self.changeSortHeader(idx));
      }

      tr.appendChild(th);
    });

    const thead = self.ownerDocument.createElement('thead');
    thead.appendChild(tr);

    self.tbody = self.ownerDocument.createElement('tbody');
    self.setSpanningText('Loading...');

    const table = self.ownerDocument.createElement('table');
    table.appendChild(thead);
    table.appendChild(self.tbody);

    self.appendChild(table);
  }

  /**
   * @param {string} text
   * @private
   */
  setSpanningText(text) {
    const td = this.ownerDocument.createElement('td');
    td.innerText = text;
    td.colSpan = this.model.cols.length;

    const tr = this.ownerDocument.createElement('tr');
    tr.appendChild(td);

    this.tbody.appendChild(tr);
  }

  /**
   * @param {!Element} th
   * @param {?boolean} sortDesc
   * @private
   */
  static setSortAttrs(th, sortDesc) {
    let nextDir;
    if (sortDesc === null) {
      th.ariaSort = 'none';
      nextDir = 'ascending';
    } else if (sortDesc) {
      th.ariaSort = 'descending';
      nextDir = 'ascending';
    } else {
      th.ariaSort = 'ascending';
      nextDir = 'descending';
    }

    th.title = `Sort by ${th.innerText} ${nextDir}`;
    th.ariaLabel = th.title;
  }

  /**
   * @param {number} idx
   * @private
   */
  changeSortHeader(idx) {
    const ths = this.querySelectorAll('thead th');

    if (idx === this.sortIdx) {
      this.sortDesc = !this.sortDesc;
    } else {
      this.sortDesc = false;
      if (this.sortIdx >= 0) {
        Table.setSortAttrs(ths[this.sortIdx], /*descending=*/ null);
      }
    }

    this.sortIdx = idx;
    Table.setSortAttrs(ths[this.sortIdx], this.sortDesc);
    this.sort();
    this.updateTbody();
  }

  /** @private */
  sort() {
    if (this.sortIdx < 0) {
      return;
    }
    this.model.sort(this.sortIdx, this.sortDesc);
  }

  /** @private */
  updateTbody() {
    this.tbody.innerText = '';

    if (this.model.rows.length === 0) {
      this.setSpanningText(this.model.emptyRowText);
      return;
    }

    this.model.rows.forEach((row) => {
      const tr = this.ownerDocument.createElement('tr');
      this.model.cols.forEach((col) => {
        const td = this.ownerDocument.createElement('td');
        col.render(td, row);
        tr.appendChild(td);
      });
      this.model.styleRow(tr, row);
      this.tbody.appendChild(tr);
    });
  }

  /**
   * @param {!Array<!T>} rows
   */
  setRows(rows) {
    this.model.rows = rows;
    this.sort();
    this.updateTbody();
  }
}

Table.prototype.__proto__ = HTMLDivElement.prototype;

class Source {
  /**
   * @param {!WebUIImpression} mojo
   */
  constructor(mojo) {
    this.impressionData = mojo.impressionData;
    this.impressionOrigin = UrlToText(mojo.impressionOrigin);
    this.conversionDestination = UrlToText(mojo.conversionDestination);
    this.reportingOrigin = UrlToText(mojo.reportingOrigin);
    this.impressionTime = new Date(mojo.impressionTime);
    this.expiryTime = new Date(mojo.expiryTime);
    this.sourceType = SourceTypeToText(mojo.sourceType);
    this.priority = mojo.priority;
    this.dedupKeys = mojo.dedupKeys.join(', ');
    this.status = mojo.reportable ? 'reportable' : 'unreportable';
  }
}

/** @extends {TableModel<Source>} */
class SourceTableModel extends TableModel {
  constructor() {
    super();

    this.cols = [
      new Column('Source Event ID', (e) => e.impressionData),
      new Column('Source Origin', (e) => e.impressionOrigin),
      new Column('Destination', (e) => e.conversionDestination),
      new Column('Report To', (e) => e.reportingOrigin),
      new DateColumn('Source Registration Time', (e) => e.impressionTime),
      new DateColumn('Expiry Time', (e) => e.expiryTime),
      new Column('Source Type', (e) => e.sourceType),
      new Column('Priority', (e) => e.priority),
      new Column('Dedup Keys', (e) => e.dedupKeys, /*compare=*/ null),
      new Column('Status', (e) => e.status),
    ];

    this.emptyRowText = 'No active sources.';
  }
}

class Report {
  /**
   * @param {!WebUIConversionReport} mojo
   */
  constructor(mojo) {
    this.reportBody = mojo.reportBody;
    this.conversionOrigin = UrlToText(mojo.conversionOrigin);
    this.reportUrl = mojo.reportUrl.url;
    this.triggerTime = new Date(mojo.triggerTime);
    this.reportTime = new Date(mojo.reportTime);
    this.reportPriority = mojo.priority;
    this.attributedTruthfully = mojo.attributedTruthfully;

    switch (mojo.status) {
      case WebUIConversionReport_Status.kSent:
        this.status = `Sent: HTTP ${mojo.httpResponseCode}`;
        this.httpResponseCode = mojo.httpResponseCode;
        break;
      case WebUIConversionReport_Status.kPending:
        this.status = 'Pending';
        break;
      case WebUIConversionReport_Status.kDroppedDueToLowPriority:
        this.status = 'Dropped due to low priority';
        break;
      case WebUIConversionReport_Status.kDroppedForNoise:
        this.status = 'Dropped for noise';
        break;
    }
  }
}

/** @extends {TableModel<Report>} */
class ReportTableModel extends TableModel {
  constructor() {
    super();

    this.cols = [
      new CodeColumn('Report Body', (e) => e.reportBody),
      new Column('Destination', (e) => e.conversionOrigin),
      new Column('Report URL', (e) => e.reportUrl),
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new DateColumn('Report Time', (e) => e.reportTime),
      new Column('Report Priority', (e) => e.reportPriority),
      new Column('Fake Report', (e) => e.attributedTruthfully ? 'no' : 'yes'),
      new Column('Status', (e) => e.status),
    ];

    this.emptyRowText = 'No sent or pending reports.';
  }

  /** @override */
  styleRow(tr, report) {
    tr.classList.toggle(
        'http-error',
        report.httpResponseCode < 200 || report.httpResponseCode >= 400);
  }
}

/**
 * Reference to the backend providing all the data.
 * @type {?ConversionInternalsHandlerRemote}
 */
let pageHandler = null;

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
 * Fetch all active sources, pending reports, and sent reports from the
 * backend and populate the tables. Also update measurement enabled status.
 */
function updatePageData() {
  // Get the feature status for ConversionMeasurement and populate it.
  pageHandler.isMeasurementEnabled().then((response) => {
    $('feature-status-content').innerText =
        response.enabled ? 'enabled' : 'disabled';
    $('feature-status-content').classList.toggle('disabled', !response.enabled);

    $('debug-mode-content').innerHTML =
        getTrustedHTML`The #conversion-measurement-debug-mode flag is
 <strong>enabled</strong>, reports are sent immediately and never pending.`;

    if (!response.debugMode) {
      $('debug-mode-content').innerText = '';
    }
  });

  pageHandler.getActiveImpressions().then((response) => {
    $('source-table-wrapper')
        .setRows(response.impressions.map((mojo) => new Source(mojo)));
  });

  pageHandler.getReports().then((response) => {
    $('report-table-wrapper')
        .setRows(response.reports.map((mojo) => new Report(mojo)));
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

  Table.decorate(
      getRequiredElement('source-table-wrapper'), new SourceTableModel());
  Table.decorate(
      getRequiredElement('report-table-wrapper'), new ReportTableModel());

  // Automatically refresh every 2 minutes.
  setInterval(updatePageData, 2 * 60 * 1000);
  updatePageData();
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {$, getRequiredElement} from 'chrome://resources/js/util.m.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {AttributionInternalsHandler, AttributionInternalsHandlerRemote, AttributionInternalsObserverInterface, AttributionInternalsObserverReceiver, SourceType, WebUIAttributionReport, WebUIAttributionReport_Status, WebUIAttributionSource, WebUIAttributionSource_Attributability} from './attribution_internals.mojom-webui.js';

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
 * @abstract
 */
class TableModel {
  constructor() {
    /** @type {?Table<T>} */
    this.table;

    /** @type {!Array<Column<T, ?>>} */
    this.cols;

    /** @type {string} */
    this.emptyRowText;

    /** @type {number} */
    this.sortIdx = -1;
  }

  /**
   * @param {!Element} tr
   * @param {T} data
   */
  styleRow(tr, data) {}

  /**
   * @abstract
   * @return {!Array<!T>}
   */
  getRows() {}
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

    model.table = self;

    self.model = model;
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

    if (idx === this.model.sortIdx) {
      this.sortDesc = !this.sortDesc;
    } else {
      this.sortDesc = false;
      if (this.model.sortIdx >= 0) {
        Table.setSortAttrs(ths[this.model.sortIdx], /*descending=*/ null);
      }
    }

    this.model.sortIdx = idx;
    Table.setSortAttrs(ths[this.model.sortIdx], this.sortDesc);
    this.updateTbody();
  }

  /**
   * @param {!Array<T>} rows
   * @private
   */
  sort(rows) {
    if (this.model.sortIdx < 0) {
      return;
    }

    const multiplier = this.sortDesc ? -1 : 1;
    rows.sort(
        (a, b) =>
            this.model.cols[this.model.sortIdx].compare(a, b) * multiplier);
  }

  updateTbody() {
    this.tbody.innerText = '';

    const rows = this.model.getRows();

    if (rows.length === 0) {
      this.setSpanningText(this.model.emptyRowText);
      return;
    }

    this.sort(rows);

    rows.forEach((row) => {
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
}

Table.prototype.__proto__ = HTMLDivElement.prototype;

class Source {
  /**
   * @param {!WebUIAttributionSource} mojo
   */
  constructor(mojo) {
    this.sourceEventId = mojo.sourceEventId;
    this.impressionOrigin = OriginToText(mojo.impressionOrigin);
    this.attributionDestination = mojo.attributionDestination;
    this.reportingOrigin = OriginToText(mojo.reportingOrigin);
    this.impressionTime = new Date(mojo.impressionTime);
    this.expiryTime = new Date(mojo.expiryTime);
    this.sourceType = SourceTypeToText(mojo.sourceType);
    this.priority = mojo.priority;
    this.dedupKeys = mojo.dedupKeys.join(', ');
    this.status = AttributabilityToText(mojo.attributability);
  }
}

/** @extends {TableModel<Source>} */
class SourceTableModel extends TableModel {
  constructor() {
    super();

    this.cols = [
      new Column('Source Event ID', (e) => e.sourceEventId),
      new Column('Source Origin', (e) => e.impressionOrigin),
      new Column('Destination', (e) => e.attributionDestination),
      new Column('Report To', (e) => e.reportingOrigin),
      new DateColumn('Source Registration Time', (e) => e.impressionTime),
      new DateColumn('Expiry Time', (e) => e.expiryTime),
      new Column('Source Type', (e) => e.sourceType),
      new Column('Priority', (e) => e.priority),
      new Column('Dedup Keys', (e) => e.dedupKeys, /*compare=*/ null),
      new Column('Status', (e) => e.status),
    ];

    this.emptyRowText = 'No sources.';

    // Sort by source registration time by default.
    this.sortIdx = 4;

    /** @type {!Array<!Source>} */
    this.deactivatedSources = [];

    /** @type {!Array<!Source>} */
    this.storedSources = [];
  }

  /** @override */
  getRows() {
    return this.deactivatedSources.concat(this.storedSources);
  }

  /** @param {!Array<!Source>} storedSources */
  setStoredSources(storedSources) {
    this.storedSources = storedSources;
    this.table.updateTbody();
  }

  /** @param {!Source} source */
  addDeactivatedSource(source) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.deactivatedSources.length >= 1000) {
      this.deactivatedSources = [];
    }

    this.deactivatedSources.push(source);
    this.table.updateTbody();
  }

  clear() {
    this.storedSources = [];
    this.deactivatedSources = [];
    this.table.updateTbody();
  }
}

class Report {
  /**
   * @param {!WebUIAttributionReport} mojo
   */
  constructor(mojo) {
    this.reportBody = mojo.reportBody;
    this.attributionDestination = mojo.attributionDestination;
    this.reportUrl = mojo.reportUrl.url;
    this.triggerTime = new Date(mojo.triggerTime);
    this.reportTime = new Date(mojo.reportTime);
    this.reportPriority = mojo.priority;
    this.attributedTruthfully = mojo.attributedTruthfully;

    switch (mojo.status) {
      case WebUIAttributionReport_Status.kSent:
        this.status = `Sent: HTTP ${mojo.httpResponseCode}`;
        this.httpResponseCode = mojo.httpResponseCode;
        break;
      case WebUIAttributionReport_Status.kPending:
        this.status = 'Pending';
        break;
      case WebUIAttributionReport_Status.kDroppedDueToLowPriority:
        this.status = 'Dropped due to low priority';
        break;
      case WebUIAttributionReport_Status.kDroppedForNoise:
        this.status = 'Dropped for noise';
        break;
      case WebUIAttributionReport_Status.kProhibitedByBrowserPolicy:
        this.status = 'Prohibited by browser policy';
        break;
      case WebUIAttributionReport_Status.kNetworkError:
        this.status = 'Network error';
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
      new Column('Destination', (e) => e.attributionDestination),
      new Column('Report URL', (e) => e.reportUrl),
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new DateColumn('Report Time', (e) => e.reportTime),
      new Column('Report Priority', (e) => e.reportPriority),
      new Column('Fake Report', (e) => e.attributedTruthfully ? 'no' : 'yes'),
      new Column('Status', (e) => e.status),
    ];

    this.emptyRowText = 'No sent or pending reports.';

    // Sort by report time by default.
    this.sortIdx = 4;

    /** @type {!Array<!Report>} */
    this.sentOrDroppedReports = [];

    /** @type {!Array<!Report>} */
    this.storedReports = [];
  }

  /** @override */
  styleRow(tr, report) {
    tr.classList.toggle(
        'http-error',
        report.httpResponseCode < 200 || report.httpResponseCode >= 400);
  }

  /** @override */
  getRows() {
    return this.sentOrDroppedReports.concat(this.storedReports);
  }

  /** @param {!Array<!Report>} storedReports */
  setStoredReports(storedReports) {
    this.storedReports = storedReports;
    this.table.updateTbody();
  }

  /** @param {!Report} report */
  addSentOrDroppedReport(report) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.sentOrDroppedReports.length >= 1000) {
      this.sentOrDroppedReports = [];
    }

    this.sentOrDroppedReports.push(report);
    this.table.updateTbody();
  }

  clear() {
    this.storedReports = [];
    this.sentOrDroppedReports = [];
    this.table.updateTbody();
  }
}

/**
 * Reference to the backend providing all the data.
 * @type {?AttributionInternalsHandlerRemote}
 */
let pageHandler = null;


/** @type {?SourceTableModel} */
let sourceTableModel = null;

/** @type {?ReportTableModel} */
let reportTableModel = null;

/**
 * Converts a mojo origin into a user-readable string, omitting default ports.
 * @param {Origin} origin Origin to convert
 * @return {string}
 */
function OriginToText(origin) {
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
 * Converts a mojo Attributability into a user-readable string.
 * @param {WebUIAttributionSource_Attributability} attributability
 *     Attributability to convert
 * @return {string}
 */
function AttributabilityToText(attributability) {
  switch (attributability) {
    case WebUIAttributionSource_Attributability.kAttributable:
      return 'Attributable';
    case WebUIAttributionSource_Attributability.kNoised:
      return 'Unattributable: noised';
    case WebUIAttributionSource_Attributability.kReplacedByNewerSource:
      return 'Unattributable: replaced by newer source';
    case WebUIAttributionSource_Attributability.kReachedAttributionLimit:
      return 'Unattributable: reached attribution limit';
    default:
      return attributability.toString();
  }
}

/**
 * Fetch all sources, pending reports, and sent reports from the
 * backend and populate the tables. Also update measurement enabled status.
 */
function updatePageData() {
  // Get the feature status for Attribution Reporting and populate it.
  pageHandler.isAttributionReportingEnabled().then((response) => {
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

  updateSources();
  updateReports();
}

function updateSources() {
  pageHandler.getActiveSources().then((response) => {
    sourceTableModel.setStoredSources(
        response.sources.map((mojo) => new Source(mojo)));
  });
}

function updateReports() {
  pageHandler.getReports().then((response) => {
    reportTableModel.setStoredReports(
        response.reports.map((mojo) => new Report(mojo)));
  });
}

/**
 * Deletes all data stored by the conversions backend.
 * Observer.onReportsChanged and Observer.onSourcesChanged will be called
 * automatically as reports are deleted, so there's no need to manually refresh
 * the data on completion.
 */
function clearStorage() {
  reportTableModel.clear();
  pageHandler.clearStorage();
}

/**
 * Sends all conversion reports.
 * Disables the button while the reports are still being sent.
 * Observer.onReportsChanged and Observer.onSourcesChanged will be called
 * automatically as reports are deleted, so there's no need to manually refresh
 * the data on completion.
 */
function sendReports() {
  const button = $('send-reports');
  const previousText = $('send-reports').innerText;

  button.disabled = true;
  button.innerText = 'Sending...';
  pageHandler.sendPendingReports().then(() => {
    button.disabled = false;
    button.innerText = previousText;
  });
}

/** @implements {AttributionInternalsObserverInterface} */
class Observer {
  /** @override */
  onSourcesChanged() {
    updateSources();
  }

  /** @override */
  onReportsChanged() {
    updateReports();
  }

  /** @override */
  onSourceDeactivated(mojo) {
    sourceTableModel.addDeactivatedSource(new Source(mojo));
  }

  /** @override */
  onReportSent(mojo) {
    reportTableModel.addSentOrDroppedReport(new Report(mojo));
  }

  /** @override */
  onReportDropped(mojo) {
    reportTableModel.addSentOrDroppedReport(new Report(mojo));
  }
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup the mojo interface.
  pageHandler = AttributionInternalsHandler.getRemote();

  sourceTableModel = new SourceTableModel();
  reportTableModel = new ReportTableModel();

  $('refresh').addEventListener('click', updatePageData);
  $('clear-data').addEventListener('click', clearStorage);
  $('send-reports').addEventListener('click', sendReports);

  Table.decorate(getRequiredElement('source-table-wrapper'), sourceTableModel);
  Table.decorate(getRequiredElement('report-table-wrapper'), reportTableModel);

  const receiver = new AttributionInternalsObserverReceiver(new Observer());
  pageHandler.addObserver(receiver.$.bindNewPipeAndPassRemote());

  updatePageData();
});

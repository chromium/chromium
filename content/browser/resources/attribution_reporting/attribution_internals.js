// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {$, getRequiredElement} from 'chrome://resources/js/util.m.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {AttributionInternalsHandler, AttributionInternalsHandlerRemote, AttributionInternalsObserverInterface, AttributionInternalsObserverReceiver, AttributionReportAggregatableAttributionID, AttributionReportEventLevelID, AttributionReportType, AttributionSourceType, WebUIAttributionReport, WebUIAttributionReport_Status, WebUIAttributionSource, WebUIAttributionSource_Attributability} from './attribution_internals.mojom-webui.js';

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
 * @abstract
 */
class Column {
  constructor() {
    /** @type {?function(!T, !T): number} */
    this.compare;
  }

  /**
   * @param {!Element} td
   * @param {!T} row
   * @abstract
   */
  render(td, row) {}

  /**
   * @param {!Element} th
   * @abstract
   */
  renderHeader(th) {}
}

/**
 * @template T
 * @template V
 * @extends {Column<T>}
 */
class ValueColumn extends Column {
  /**
   * @param {string} header
   * @param {function(!T): !V} getValue
   * @param {?function(!T, !T): number} compare
   */
  constructor(
      header, getValue,
      compare = (a, b) => compareDefault(getValue(a), getValue(b))) {
    super();

    this.header = header;

    /** @protected */
    this.getValue = getValue;

    this.compare = compare;
  }

  /** @override */
  render(td, row) {
    td.innerText = this.getValue(row);
  }

  /** @override */
  renderHeader(th) {
    th.innerText = this.header;
  }
}

/**
 * @template T
 * @extends {ValueColumn<T, Date>}
 */
class DateColumn extends ValueColumn {
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
 * @extends {ValueColumn<T, string>}
 */
class CodeColumn extends ValueColumn {
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
    /** @type {!Array<Column<T>>} */
    this.cols;

    /** @type {string} */
    this.emptyRowText;

    /** @type {number} */
    this.sortIdx = -1;

    /** @type {!Set<function()>} */
    this.rowsChangedListeners = new Set();
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

  notifyRowsChanged() {
    this.rowsChangedListeners.forEach((f) => f());
  }
}

class Selectable {
  constructor() {
    this.input = document.createElement('input');
    this.input.type = 'checkbox';
  }
}

/**
 * @template T
 * @extends {Column<T>}
 */
class SelectionColumn extends Column {
  /**
   * @param {!TableModel<T>} model
   */
  constructor(model) {
    super();

    this.model = model;

    this.selectAll = document.createElement('input');
    this.selectAll.type = 'checkbox';
    this.selectAll.addEventListener('input', () => {
      const checked = this.selectAll.checked;
      this.model.getRows().forEach((row) => {
        if (!row.input.disabled) {
          row.input.checked = checked;
        }
      });
      this.notifySelectionChanged(checked);
    });

    this.listener = () => this.onChange();
    this.model.rowsChangedListeners.add(this.listener);

    /** @type {!Set<function(boolean)>} */
    this.selectionChangedListeners = new Set();
  }

  /** @override */
  render(td, row) {
    td.appendChild(row.input);
  }

  /** @override */
  renderHeader(th) {
    th.appendChild(this.selectAll);
  }

  onChange() {
    let anySelectable = false;
    let anySelected = false;
    let anyUnselected = false;

    this.model.getRows().forEach((row) => {
      // addEventListener deduplicates, so only one event will be fired per
      // input.
      row.input.addEventListener('input', this.listener);

      if (row.input.disabled) {
        return;
      }

      anySelectable = true;

      if (row.input.checked) {
        anySelected = true;
      } else {
        anyUnselected = true;
      }
    });

    this.selectAll.disabled = !anySelectable;
    this.selectAll.checked = anySelected && !anyUnselected;
    this.selectAll.indeterminate = anySelected && anyUnselected;

    this.notifySelectionChanged(anySelected);
  }

  /** @param {boolean} anySelected */
  notifySelectionChanged(anySelected) {
    this.selectionChangedListeners.forEach((f) => f(anySelected));
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
    self.sortDesc = false;

    const tr = self.ownerDocument.createElement('tr');
    self.model.cols.forEach((col, idx) => {
      const th = self.ownerDocument.createElement('th');
      th.scope = 'col';
      col.renderHeader(th);

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

    self.model.rowsChangedListeners.add(() => self.updateTbody());
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
    this.filterData = JSON.stringify(mojo.filterData, null, ' ');
    this.debugKey = mojo.debugKey ? mojo.debugKey.value : '';
    this.dedupKeys = mojo.dedupKeys.join(', ');
    this.status = AttributabilityToText(mojo.attributability);
  }
}

/** @extends {TableModel<Source>} */
class SourceTableModel extends TableModel {
  constructor() {
    super();

    this.cols = [
      new ValueColumn('Source Event ID', (e) => e.sourceEventId),
      new ValueColumn('Source Origin', (e) => e.impressionOrigin),
      new ValueColumn('Destination', (e) => e.attributionDestination),
      new ValueColumn('Report To', (e) => e.reportingOrigin),
      new DateColumn('Source Registration Time', (e) => e.impressionTime),
      new DateColumn('Expiry Time', (e) => e.expiryTime),
      new ValueColumn('Source Type', (e) => e.sourceType),
      new ValueColumn('Priority', (e) => e.priority),
      new CodeColumn('Filter Data', (e) => e.filterData),
      new ValueColumn('Debug Key', (e) => e.debugKey),
      new ValueColumn('Dedup Keys', (e) => e.dedupKeys, /*compare=*/ null),
      new ValueColumn('Status', (e) => e.status),
    ];

    this.emptyRowText = 'No sources.';

    // Sort by source registration time by default.
    this.sortIdx = 4;

    /** @type {!Array<!Source>} */
    this.unstoredSources = [];

    /** @type {!Array<!Source>} */
    this.storedSources = [];
  }

  /** @override */
  getRows() {
    return this.unstoredSources.concat(this.storedSources);
  }

  /** @param {!Array<!Source>} storedSources */
  setStoredSources(storedSources) {
    this.storedSources = storedSources;
    this.notifyRowsChanged();
  }

  /** @param {!Source} source */
  addUnstoredSource(source) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.unstoredSources.length >= 1000) {
      this.unstoredSources = [];
    }

    this.unstoredSources.push(source);
    this.notifyRowsChanged();
  }

  clear() {
    this.storedSources = [];
    this.unstoredSources = [];
    this.notifyRowsChanged();
  }
}

/**
 * @template ID
 */
class Report extends Selectable {
  /**
   * @param {!WebUIAttributionReport} mojo
   * @param {ID} id
   */
  constructor(mojo, id) {
    super();

    this.id = id;
    this.reportBody = mojo.reportBody;
    this.attributionDestination = mojo.attributionDestination;
    this.reportUrl = mojo.reportUrl.url;
    this.triggerTime = new Date(mojo.triggerTime);
    this.reportTime = new Date(mojo.reportTime);

    // Only pending reports are selectable.
    if (this.id === null ||
        mojo.status !== WebUIAttributionReport_Status.kPending) {
      this.input.disabled = true;
    }

    switch (mojo.status) {
      case WebUIAttributionReport_Status.kSent:
        this.status = `Sent: HTTP ${mojo.httpResponseCode}`;
        this.httpResponseCode = mojo.httpResponseCode;
        break;
      case WebUIAttributionReport_Status.kPending:
        this.status = 'Pending';
        break;
      case WebUIAttributionReport_Status.kDroppedDueToExcessiveAttributions:
        this.status = 'Dropped due to excessive attributions';
        break;
      case WebUIAttributionReport_Status.kDroppedDueToExcessiveReportingOrigins:
        this.status = 'Dropped due to excessive reporting origins';
        break;
      case WebUIAttributionReport_Status.kDroppedDueToLowPriority:
        this.status = 'Dropped due to low priority';
        break;
      case WebUIAttributionReport_Status.kDroppedForNoise:
        this.status = 'Dropped for noise';
        break;
      case WebUIAttributionReport_Status.kDeduplicated:
        this.status = 'Deduplicated';
        break;
      case WebUIAttributionReport_Status.kNoReportCapacityForDestinationSite:
        this.status = 'No report capacity for destination site';
        break;
      case WebUIAttributionReport_Status.kInternalError:
        this.status = 'Internal error';
        break;
      case WebUIAttributionReport_Status.kProhibitedByBrowserPolicy:
        this.status = 'Prohibited by browser policy';
        break;
      case WebUIAttributionReport_Status.kNetworkError:
        this.status = 'Network error';
        break;
      case WebUIAttributionReport_Status.kNoMatchingSourceFilterData:
        this.status = 'Dropped due to no matching source filter data';
        break;
      case WebUIAttributionReport_Status.kFailedToAssemble:
        this.status = 'Dropped due to assembly failure';
        break;
      case WebUIAttributionReport_Status.kInsufficientAggregatableBudget:
        this.status = 'Dropped due to insufficient aggregatable budget';
        break;
    }
  }
}

/** @extends {Report<!AttributionReportEventLevelID>} */
class EventLevelReport extends Report {
  /**
   * @param {!WebUIAttributionReport} mojo
   */
  constructor(mojo) {
    super(mojo, mojo.data.eventLevelData.id);

    this.reportPriority = mojo.data.eventLevelData.priority;
    this.attributedTruthfully = mojo.data.eventLevelData.attributedTruthfully;
  }
}

/** @extends {Report<!AttributionReportAggregatableAttributionID>} */
class AggregatableAttributionReport extends Report {
  /**
   * @param {!WebUIAttributionReport} mojo
   */
  constructor(mojo) {
    super(mojo, mojo.data.aggregatableAttributionData.id);

    this.contributions = JSON.stringify(
        mojo.data.aggregatableAttributionData.contributions,
        (key, value) =>
            typeof value === 'bigint' ? value.toString() : value, ' ');
  }
}

/**
 * @template ID
 * @extends {TableModel<Report<ID>>}
 */
class ReportTableModel extends TableModel {
  constructor() {
    super();

    this.selectionColumn = new SelectionColumn(this);

    this.emptyRowText = 'No sent or pending reports.';

    /** @type {!Array<!Report<ID>>} */
    this.sentOrDroppedReports = [];

    /** @type {!Array<!Report<ID>>} */
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

  /** @param {!Array<!Report<ID>>} storedReports */
  setStoredReports(storedReports) {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  /** @param {!Report<ID>} report */
  addSentOrDroppedReport(report) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.sentOrDroppedReports.length >= 1000) {
      this.sentOrDroppedReports = [];
    }

    this.sentOrDroppedReports.push(report);
    this.notifyRowsChanged();
  }

  clear() {
    this.storedReports = [];
    this.sentOrDroppedReports = [];
    this.notifyRowsChanged();
  }

  /**
   * @return {Array<ID>}
   */
  getSelectedIDs() {
    const ids = [];
    this.storedReports.forEach((report) => {
      if (!report.input.disabled && report.input.checked && report.id !== null) {
        ids.push(report.id);
      }
    });
    return ids;
  }
}

/** @extends {ReportTableModel<!AttributionReportEventLevelID>} */
class EventLevelReportTableModel extends ReportTableModel {
  constructor() {
    super();

    this.cols = [
      this.selectionColumn,
      new CodeColumn('Report Body', (e) => e.reportBody),
      new ValueColumn('Destination', (e) => e.attributionDestination),
      new ValueColumn('Report URL', (e) => e.reportUrl),
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new DateColumn('Report Time', (e) => e.reportTime),
      new ValueColumn('Report Priority', (e) => e.reportPriority),
      new ValueColumn(
          'Fake Report', (e) => e.attributedTruthfully ? 'no' : 'yes'),
      new ValueColumn('Status', (e) => e.status),
    ];

    // Sort by report time by default.
    this.sortIdx = 5;
  }
}

/** @extends {ReportTableModel<!AttributionReportAggregatableAttributionID>} */
class AggregatableAttributionReportTableModel extends ReportTableModel {
  constructor() {
    super();

    this.cols = [
      this.selectionColumn,
      new CodeColumn('Report Body', (e) => e.reportBody),
      new ValueColumn('Destination', (e) => e.attributionDestination),
      new ValueColumn('Report URL', (e) => e.reportUrl),
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new DateColumn('Report Time', (e) => e.reportTime),
      new CodeColumn('Histograms', (e) => e.contributions),
      new ValueColumn('Status', (e) => e.status),
    ];

    // Sort by report time by default.
    this.sortIdx = 5;
  }
}


/**
 * Reference to the backend providing all the data.
 * @type {?AttributionInternalsHandlerRemote}
 */
let pageHandler = null;


/** @type {?SourceTableModel} */
let sourceTableModel = null;

/** @type {?EventLevelReportTableModel} */
let eventLevelReportTableModel = null;

/** @type {?AggregatableAttributionReportTableModel} */
let aggregatableAttributionReportTableModel = null;

/**
 * Converts a mojo origin into a user-readable string, omitting default ports.
 * @param {Origin} origin Origin to convert
 * @return {string}
 */
function OriginToText(origin) {
  if (origin.host.length === 0) {
    return 'Null';
  }

  let result = origin.scheme + '://' + origin.host;

  if ((origin.scheme === 'https' && origin.port !== 443) ||
      (origin.scheme === 'http' && origin.port !== 80)) {
    result += ':' + origin.port;
  }
  return result;
}

/**
 * Converts a mojo AttributionSourceType into a user-readable string.
 * @param {AttributionSourceType} sourceType Source type to convert
 * @return {string}
 */
function SourceTypeToText(sourceType) {
  switch (sourceType) {
    case AttributionSourceType.kNavigation:
      return 'Navigation';
    case AttributionSourceType.kEvent:
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
    case WebUIAttributionSource_Attributability.kReachedEventLevelAttributionLimit:
      return 'Attributable: reached event-level attribution limit';
    case WebUIAttributionSource_Attributability.kInternalError:
      return 'Rejected: internal error';
    case WebUIAttributionSource_Attributability.kInsufficientSourceCapacity:
      return 'Rejected: insufficient source capacity';
    case WebUIAttributionSource_Attributability
        .kInsufficientUniqueDestinationCapacity:
      return 'Rejected: insufficient unique destination capacity';
    case WebUIAttributionSource_Attributability.kExcessiveReportingOrigins:
      return 'Rejected: excessive reporting origins';
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
  updateReports(AttributionReportType.kEventLevel);
  updateReports(AttributionReportType.kAggregatableAttribution);
}

function updateSources() {
  pageHandler.getActiveSources().then((response) => {
    sourceTableModel.setStoredSources(
        response.sources.map((mojo) => new Source(mojo)));
  });
}

/**
 * @param {!AttributionReportType} reportType
 */
function updateReports(reportType) {
  pageHandler.getReports(reportType).then((response) => {
    switch (reportType) {
      case AttributionReportType.kEventLevel:
        eventLevelReportTableModel.setStoredReports(
            response.reports
                .filter((mojo) => mojo.data.eventLevelData !== undefined)
                .map((mojo) => new EventLevelReport(mojo)));
        break;
      case AttributionReportType.kAggregatableAttribution:
        aggregatableAttributionReportTableModel.setStoredReports(
            response.reports
                .filter(
                    (mojo) =>
                        mojo.data.aggregatableAttributionData !== undefined)
                .map((mojo) => new AggregatableAttributionReport(mojo)));
        break;
    }
  });
}

/**
 * Deletes all data stored by the conversions backend.
 * Observer.onReportsChanged and Observer.onSourcesChanged will be called
 * automatically as reports are deleted, so there's no need to manually refresh
 * the data on completion.
 */
function clearStorage() {
  sourceTableModel.clear();
  eventLevelReportTableModel.clear();
  aggregatableAttributionReportTableModel.clear();
  pageHandler.clearStorage();
}

/**
 * Sends all conversion reports.
 * Disables the button while the reports are still being sent.
 * Observer.onReportsChanged and Observer.onSourcesChanged will be called
 * automatically as reports are deleted, so there's no need to manually refresh
 * the data on completion.
 */
function sendEventLevelReports() {
  const ids = eventLevelReportTableModel.getSelectedIDs();

  if (ids.length === 0) {
    return;
  }

  const button = $('send-reports');
  const previousText = button.innerText;

  button.disabled = true;
  button.innerText = 'Sending...';

  pageHandler.sendEventLevelReports(ids).then(() => {
    button.innerText = previousText;
  });
}

function sendAggregatableAttributionReports() {
  const ids = aggregatableAttributionReportTableModel.getSelectedIDs();

  if (ids.length === 0) {
    return;
  }

  const button = $('send-aggregatable-reports');
  const previousText = $('send-aggregatable-reports').innerText;

  button.disabled = true;
  button.innerText = 'Sending...';

  pageHandler.sendAggregatableAttributionReports(ids).then(() => {
    button.innerText = previousText;
  });
}

/**
 * @param {!WebUIAttributionReport} mojo
 */
function addSentOrDroppedReport(mojo) {
  if (mojo.data.eventLevelData !== undefined) {
    eventLevelReportTableModel.addSentOrDroppedReport(
        new EventLevelReport(mojo));
  } else {
    aggregatableAttributionReportTableModel.addSentOrDroppedReport(
        new AggregatableAttributionReport(mojo));
  }
}

/** @implements {AttributionInternalsObserverInterface} */
class Observer {
  /** @override */
  onSourcesChanged() {
    updateSources();
  }

  /** @override */
  onReportsChanged(reportType) {
    updateReports(reportType);
  }

  /** @override */
  onSourceRejectedOrDeactivated(mojo) {
    sourceTableModel.addUnstoredSource(new Source(mojo));
  }

  /** @override */
  onReportSent(mojo) {
    addSentOrDroppedReport(mojo);
  }

  /** @override */
  onReportDropped(mojo) {
    addSentOrDroppedReport(mojo);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup the mojo interface.
  pageHandler = AttributionInternalsHandler.getRemote();

  sourceTableModel = new SourceTableModel();
  eventLevelReportTableModel = new EventLevelReportTableModel();
  aggregatableAttributionReportTableModel =
      new AggregatableAttributionReportTableModel();

  $('refresh').addEventListener('click', updatePageData);
  $('clear-data').addEventListener('click', clearStorage);

  const sendEventLevelReportsButton = $('send-reports');
  sendEventLevelReportsButton.addEventListener('click', sendEventLevelReports);
  eventLevelReportTableModel.selectionColumn.selectionChangedListeners.add(
      (anySelected) => {
        sendEventLevelReportsButton.disabled = !anySelected;
      });

  const sendAggregatableAttributionReportsButton =
      $('send-aggregatable-reports');
  sendAggregatableAttributionReportsButton.addEventListener(
      'click', sendAggregatableAttributionReports);
  aggregatableAttributionReportTableModel.selectionColumn
      .selectionChangedListeners.add((anySelected) => {
        sendAggregatableAttributionReportsButton.disabled = !anySelected;
      });

  Table.decorate(getRequiredElement('source-table-wrapper'), sourceTableModel);
  Table.decorate(
      getRequiredElement('report-table-wrapper'), eventLevelReportTableModel);
  Table.decorate(
      getRequiredElement('aggregatable-report-table-wrapper'),
      aggregatableAttributionReportTableModel);

  const receiver = new AttributionInternalsObserverReceiver(new Observer());
  pageHandler.addObserver(receiver.$.bindNewPipeAndPassRemote());

  updatePageData();
});

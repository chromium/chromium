// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {$, getRequiredElement, queryRequiredElement} from 'chrome://resources/js/util.m.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {Handler as AttributionInternalsHandler, HandlerRemote as AttributionInternalsHandlerRemote, ObserverInterface, ObserverReceiver, ReportStatus, ReportType, SourceType, WebUIReport, WebUISource, WebUISource_Attributability, WebUITrigger, WebUITrigger_Status} from './attribution_internals.mojom-webui.js';

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
 * @param {string} key
 * @param {*} value
 * @return {*}
 */
function bigint_replacer(key, value) {
  return typeof value === 'bigint' ? value.toString() : value;
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

const debugPathPattern =
    /(?<=\/\.well-known\/attribution-reporting\/)debug(?=\/)/;

/**
 * @extends {ValueColumn<Report, string>}
 */
class ReportUrlColumn extends ValueColumn {
  constructor() {
    super('Report URL', (e) => e.reportUrl);
  }

  /** @override */
  render(td, row) {
    if (!row.isDebug) {
      td.innerText = row.reportUrl;
      return;
    }

    const [pre, post] = row.reportUrl.split(debugPathPattern, 2);
    td.appendChild(new Text(pre));

    const span = td.ownerDocument.createElement('span');
    span.classList.add('debug-url');
    span.innerText = 'debug';
    td.appendChild(span);

    td.appendChild(new Text(post));
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
    self.setSpanningText(self.model.emptyRowText);

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
   * @param {!WebUISource} mojo
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
    this.aggregatableSource = JSON.stringify(mojo.aggregatableSource, bigint_replacer, ' ');
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
      new ValueColumn('Status', (e) => e.status),
      new ValueColumn('Source Origin', (e) => e.impressionOrigin),
      new ValueColumn('Destination', (e) => e.attributionDestination),
      new ValueColumn('Report To', (e) => e.reportingOrigin),
      new DateColumn('Source Registration Time', (e) => e.impressionTime),
      new DateColumn('Expiry Time', (e) => e.expiryTime),
      new ValueColumn('Source Type', (e) => e.sourceType),
      new ValueColumn('Priority', (e) => e.priority),
      new CodeColumn('Filter Data', (e) => e.filterData),
      new CodeColumn('Aggregatable Source', (e) => e.aggregatableSource),
      new ValueColumn('Debug Key', (e) => e.debugKey),
      new ValueColumn('Dedup Keys', (e) => e.dedupKeys, /*compare=*/ null),
    ];

    this.emptyRowText = 'No sources.';

    // Sort by source registration time by default.
    this.sortIdx = 5;

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

class Trigger {
  /**
   * @param {!WebUITrigger} mojo
   */
  constructor(mojo) {
    this.triggerTime = new Date(mojo.triggerTime);
    this.destinationOrigin = OriginToText(mojo.destinationOrigin);
    this.reportingOrigin = OriginToText(mojo.reportingOrigin);
    this.filters = JSON.stringify(mojo.filters, null, ' ');
    this.debugKey = mojo.debugKey ? mojo.debugKey.value : '';

    this.eventTriggers = JSON.stringify(
        mojo.eventTriggers.map((e) => {
          // Omit the dedup key, filters, and not filters if they are empty for
          // brevity.
          return {
            'data': e.data,
            'priority': e.priority,
            'deduplication_key': e.dedupKey ? e.dedupKey.value : undefined,
            'filters': Object.entries(e.filters).length > 0 ? e.filters :
                                                              undefined,
            'not_filters': Object.entries(e.notFilters).length > 0 ?
                e.notFilters :
                undefined,
          };
        }),
        bigint_replacer, ' ');

    this.eventLevelStatus = TriggerStatusToText(mojo.eventLevelStatus);
    this.aggregatableStatus = TriggerStatusToText(mojo.aggregatableStatus);
  }
}

/** @extends {TableModel<Trigger>} */
class TriggerTableModel extends TableModel {
  constructor() {
    super();

    this.cols = [
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new ValueColumn('Event-Level Status', (e) => e.eventLevelStatus),
      new ValueColumn('Aggregatable Status', (e) => e.aggregatableStatus),
      new ValueColumn('Destination', (e) => e.destinationOrigin),
      new ValueColumn('Report To', (e) => e.reportingOrigin),
      new ValueColumn('Debug Key', (e) => e.debugKey),
      new CodeColumn('Filters', (e) => e.filters),
      new CodeColumn('Event Triggers', (e) => e.eventTriggers),
    ];

    this.emptyRowText = 'No triggers.';

    // Sort by trigger time by default.
    this.sortIdx = 0;

    /** @type {!Array<!Trigger>} */
    this.triggers = [];
  }

  /** @override */
  getRows() {
    return this.triggers;
  }

  /** @param {!Trigger} trigger */
  addTrigger(trigger) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.triggers.length >= 1000) {
      this.triggers = [];
    }

    this.triggers.push(trigger);
    this.notifyRowsChanged();
  }

  clear() {
    this.triggers = [];
    this.notifyRowsChanged();
  }
}

class Report extends Selectable {
  /**
   * @param {!WebUIReport} mojo
   */
  constructor(mojo) {
    super();

    this.id = mojo.id;
    this.reportBody = mojo.reportBody;
    this.reportUrl = mojo.reportUrl.url;
    this.triggerTime = new Date(mojo.triggerTime);
    this.reportTime = new Date(mojo.reportTime);

    // Only pending reports are selectable.
    if (this.id === null || mojo.status.pending === undefined) {
      this.input.disabled = true;
    }

    this.isDebug = this.reportUrl.indexOf(
                       '/.well-known/attribution-reporting/debug/') >= 0;

    if (mojo.status.sent !== undefined) {
      this.status = `Sent: HTTP ${mojo.status.sent}`;
      this.httpResponseCode = mojo.status.sent;
    } else if (mojo.status.pending !== undefined) {
      this.status = 'Pending';
    } else if (mojo.status.replacedByHigherPriorityReport !== undefined) {
      this.status = 'Replaced by higher-priority report';
    } else if (mojo.status.prohibitedByBrowserPolicy !== undefined) {
      this.status = 'Prohibited by browser policy';
    } else if (mojo.status.networkError !== undefined) {
      this.status = `Network error: ${mojo.status.networkError}`;
    } else if (mojo.status.failedToAssemble !== undefined) {
      this.status = 'Dropped due to assembly failure';
    } else {
      throw new Error('invalid ReportStatus union');
    }
  }
}

/** @extends {Report} */
class EventLevelReport extends Report {
  /**
   * @param {!WebUIReport} mojo
   */
  constructor(mojo) {
    super(mojo);

    this.reportPriority = mojo.data.eventLevelData.priority;
    this.attributedTruthfully = mojo.data.eventLevelData.attributedTruthfully;
  }
}

/** @extends {Report} */
class AggregatableAttributionReport extends Report {
  /**
   * @param {!WebUIReport} mojo
   */
  constructor(mojo) {
    super(mojo);

    this.contributions = JSON.stringify(
        mojo.data.aggregatableAttributionData.contributions, bigint_replacer, ' ');
  }
}

/**
 * @extends {TableModel<Report>}
 */
class ReportTableModel extends TableModel {
  /**
   * @param {!Element} showDebugReportsContainer
   * @param {!Element} sendReportsButton
   */
  constructor(showDebugReportsContainer, sendReportsButton) {
    super();

    this.showDebugReportsCheckbox = queryRequiredElement(
        'input[type="checkbox"]', showDebugReportsContainer);
    this.hiddenDebugReportsSpan =
        queryRequiredElement('span', showDebugReportsContainer);

    this.sendReportsButton = sendReportsButton;

    this.selectionColumn = new SelectionColumn(this);

    this.emptyRowText = 'No sent or pending reports.';

    /** @type {!Array<!Report>} */
    this.sentOrDroppedReports = [];

    /** @type {!Array<!Report>} */
    this.storedReports = [];

    /** @type {!Array<!Report>} */
    this.debugReports = [];

    this.showDebugReportsCheckbox.addEventListener(
        'input', () => this.notifyRowsChanged());

    this.sendReportsButton.addEventListener('click', () => this.sendReports());
    this.selectionColumn.selectionChangedListeners.add((anySelected) => {
      this.sendReportsButton.disabled = !anySelected;
    });

    this.rowsChangedListeners.add(() => this.updateHiddenDebugReportsSpan());
  }

  /** @override */
  styleRow(tr, report) {
    tr.classList.toggle(
        'http-error',
        report.httpResponseCode < 200 || report.httpResponseCode >= 400);
  }

  /** @override */
  getRows() {
    let rows = this.sentOrDroppedReports.concat(this.storedReports);
    if (this.showDebugReportsCheckbox.checked) {
      rows = rows.concat(this.debugReports);
    }
    return rows;
  }

  /** @param {!Array<!Report>} storedReports */
  setStoredReports(storedReports) {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  /** @param {!Report} report */
  addSentOrDroppedReport(report) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.sentOrDroppedReports.length + this.debugReports.length >= 1000) {
      this.sentOrDroppedReports = [];
      this.debugReports = [];
    }

    if (report.isDebug) {
      this.debugReports.push(report);
    } else {
      this.sentOrDroppedReports.push(report);
    }

    this.notifyRowsChanged();
  }

  clear() {
    this.storedReports = [];
    this.sentOrDroppedReports = [];
    this.debugReports = [];
    this.notifyRowsChanged();
  }

  /** @private */
  updateHiddenDebugReportsSpan() {
    this.hiddenDebugReportsSpan.innerText =
        this.showDebugReportsCheckbox.checked ?
        '' :
        ` (${this.debugReports.length} hidden)`;
  }

  /**
   * Sends all selected reports.
   * Disables the button while the reports are still being sent.
   * Observer.onReportsChanged and Observer.onSourcesChanged will be called
   * automatically as reports are deleted, so there's no need to manually refresh
   * the data on completion.
   * @private
   */
  sendReports() {
    const ids = [];
    this.storedReports.forEach((report) => {
      if (!report.input.disabled && report.input.checked && report.id !== null) {
        ids.push(report.id);
      }
    });

    if (ids.length === 0) {
      return;
    }

    const previousText = this.sendReportsButton.innerText;

    this.sendReportsButton.disabled = true;
    this.sendReportsButton.innerText = 'Sending...';

    pageHandler.sendReports(ids).then(() => {
      this.sendReportsButton.innerText = previousText;
    });
  }
}

/** @extends {ReportTableModel} */
class EventLevelReportTableModel extends ReportTableModel {
  /**
   * @param {!Element} showDebugReportsContainer
   * @param {!Element} sendReportsButton
   */
  constructor(showDebugReportsContainer, sendReportsButton) {
    super(showDebugReportsContainer, sendReportsButton);

    this.cols = [
      this.selectionColumn,
      new CodeColumn('Report Body', (e) => e.reportBody),
      new ValueColumn('Status', (e) => e.status),
      new ReportUrlColumn(),
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new DateColumn('Report Time', (e) => e.reportTime),
      new ValueColumn('Report Priority', (e) => e.reportPriority),
      new ValueColumn(
          'Randomized Report', (e) => e.attributedTruthfully ? 'no' : 'yes'),
    ];

    // Sort by report time by default.
    this.sortIdx = 5;
  }
}

/** @extends {ReportTableModel} */
class AggregatableAttributionReportTableModel extends ReportTableModel {
  /**
   * @param {!Element} showDebugReportsContainer
   * @param {!Element} sendReportsButton
   */
  constructor(showDebugReportsContainer, sendReportsButton) {
    super(showDebugReportsContainer, sendReportsButton);

    this.cols = [
      this.selectionColumn,
      new CodeColumn('Report Body', (e) => e.reportBody),
      new ValueColumn('Status', (e) => e.status),
      new ReportUrlColumn(),
      new DateColumn('Trigger Time', (e) => e.triggerTime),
      new DateColumn('Report Time', (e) => e.reportTime),
      new CodeColumn('Histograms', (e) => e.contributions),
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

/** @type {?TriggerTableModel} */
let triggerTableModel = null;

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
 * @param {WebUISource_Attributability} attributability
 *     Attributability to convert
 * @return {string}
 */
function AttributabilityToText(attributability) {
  switch (attributability) {
    case WebUISource_Attributability.kAttributable:
      return 'Attributable';
    case WebUISource_Attributability.kNoised:
      return 'Unattributable: noised';
    case WebUISource_Attributability.kReplacedByNewerSource:
      return 'Unattributable: replaced by newer source';
    case WebUISource_Attributability.kReachedEventLevelAttributionLimit:
      return 'Attributable: reached event-level attribution limit';
    case WebUISource_Attributability.kInternalError:
      return 'Rejected: internal error';
    case WebUISource_Attributability.kInsufficientSourceCapacity:
      return 'Rejected: insufficient source capacity';
    case WebUISource_Attributability
        .kInsufficientUniqueDestinationCapacity:
      return 'Rejected: insufficient unique destination capacity';
    case WebUISource_Attributability.kExcessiveReportingOrigins:
      return 'Rejected: excessive reporting origins';
    case WebUISource_Attributability.kProhibitedByBrowserPolicy:
      return 'Rejected: prohibited by browser policy';
    default:
      return attributability.toString();
  }
}

/**
 * @param {WebUITrigger_Status} status
 * @return {string}
 */
function TriggerStatusToText(status) {
  switch (status) {
    case WebUITrigger_Status.kSuccess:
      return 'Success: Report stored';
    case WebUITrigger_Status.kInternalError:
      return 'Failure: Internal error';
    case WebUITrigger_Status.kNoMatchingSources:
      return 'Failure: No matching sources';
    case WebUITrigger_Status.kNoMatchingSourceFilterData:
      return 'Failure: No matching source filter data';
    case WebUITrigger_Status.kNoReportCapacityForDestinationSite:
      return 'Failure: No report capacity for destination site';
    case WebUITrigger_Status.kExcessiveAttributions:
      return 'Failure: Excessive attributions';
    case WebUITrigger_Status.kExcessiveReportingOrigins:
      return 'Failure: Excessive reporting origins';
    case WebUITrigger_Status.kDeduplicated:
      return 'Failure: Deduplicated against an earlier report';
    case WebUITrigger_Status.kLowPriority:
      return 'Failure: Priority too low';
    case WebUITrigger_Status.kNoised:
      return 'Failure: Noised';
    case WebUITrigger_Status.kNoHistograms:
      return 'Failure: No source histograms';
    case WebUITrigger_Status.kInsufficientBudget:
      return 'Failure: Insufficient budget';
    case WebUITrigger_Status.kNotRegistered:
      return 'Failure: No aggregatable data present';
    case WebUITrigger_Status.kProhibitedByBrowserPolicy:
      return 'Failure: Prohibited by browser policy';
    default:
      return status.toString();
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
  updateReports(ReportType.kEventLevel);
  updateReports(ReportType.kAggregatableAttribution);
}

function updateSources() {
  pageHandler.getActiveSources().then((response) => {
    sourceTableModel.setStoredSources(
        response.sources.map((mojo) => new Source(mojo)));
  });
}

/**
 * @param {!ReportType} reportType
 */
function updateReports(reportType) {
  pageHandler.getReports(reportType).then((response) => {
    switch (reportType) {
      case ReportType.kEventLevel:
        eventLevelReportTableModel.setStoredReports(
            response.reports
                .filter((mojo) => mojo.data.eventLevelData !== undefined)
                .map((mojo) => new EventLevelReport(mojo)));
        break;
      case ReportType.kAggregatableAttribution:
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
  triggerTableModel.clear();
  eventLevelReportTableModel.clear();
  aggregatableAttributionReportTableModel.clear();
  pageHandler.clearStorage();
}

/**
 * @param {!WebUIReport} mojo
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

/** @implements {ObserverInterface} */
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

  /** @override */
  onTriggerHandled(mojo) {
    triggerTableModel.addTrigger(new Trigger(mojo));
  }
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup the mojo interface.
  pageHandler = AttributionInternalsHandler.getRemote();

  sourceTableModel = new SourceTableModel();
  triggerTableModel = new TriggerTableModel();
  eventLevelReportTableModel = new EventLevelReportTableModel(
      getRequiredElement('show-debug-event-reports'),
      getRequiredElement('send-reports'));
  aggregatableAttributionReportTableModel =
      new AggregatableAttributionReportTableModel(
          getRequiredElement('show-debug-aggregatable-reports'),
          getRequiredElement('send-aggregatable-reports'));

  $('refresh').addEventListener('click', updatePageData);
  $('clear-data').addEventListener('click', clearStorage);

  Table.decorate(getRequiredElement('source-table-wrapper'), sourceTableModel);
  Table.decorate(
      getRequiredElement('trigger-table-wrapper'), triggerTableModel);
  Table.decorate(
      getRequiredElement('report-table-wrapper'), eventLevelReportTableModel);
  Table.decorate(
      getRequiredElement('aggregatable-report-table-wrapper'),
      aggregatableAttributionReportTableModel);

  decorate('tabbox', TabBox);

  const receiver = new ObserverReceiver(new Observer());
  pageHandler.addObserver(receiver.$.bindNewPipeAndPassRemote());

  updatePageData();
});

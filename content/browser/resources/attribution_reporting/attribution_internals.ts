// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './attribution_internals_table.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {ClearedDebugKey, ClearedDebugKey_Type, FailedSourceRegistration, Handler as AttributionInternalsHandler, HandlerRemote as AttributionInternalsHandlerRemote, ObserverInterface, ObserverReceiver, ReportID, WebUIReport, WebUISource, WebUISource_Attributability, WebUITrigger, WebUITrigger_Status} from './attribution_internals.mojom-webui.js';
import {AttributionInternalsTableElement} from './attribution_internals_table.js';
import {ReportType, SourceType} from './attribution_reporting.mojom-webui.js';
import {SourceRegistrationError} from './source_registration_error.mojom-webui.js';
import {Column, TableModel} from './table_model.js';

// If kAttributionAggregatableBudgetPerSource changes, update this value
const BUDGET_PER_SOURCE = 65536;

function compareDefault<T>(a: T, b: T): number {
  if (a < b) {
    return -1;
  }
  if (a > b) {
    return 1;
  }
  return 0;
}

function bigintReplacer(_key: string, value: any): any {
  return typeof value === 'bigint' ? value.toString() : value;
}

class ValueColumn<T, V> implements Column<T> {
  compare: (a: T, b: T) => number;
  header: string;
  protected getValue: (param: T) => V;

  constructor(
      header: string, getValue: (param: T) => V,
      compare?: ((a: T, b: T) => number)) {
    this.header = header;
    this.getValue = getValue;
    if (compare) {
      this.compare = compare;
    } else {
      this.compare = (a: T, b: T) => compareDefault(getValue(a), getValue(b));
    }
  }

  render(td: HTMLElement, row: T) {
    td.innerText = `${this.getValue(row)}`;
  }

  renderHeader(th: HTMLElement) {
    th.innerText = this.header;
  }
}

class DateColumn<T> extends ValueColumn<T, Date> {
  constructor(header: string, getValue: (p: T) => Date) {
    super(header, getValue);
  }

  override render(td: HTMLElement, row: T) {
    td.innerText = this.getValue(row).toLocaleString();
  }
}

class CodeColumn<T> extends ValueColumn<T, string> {
  constructor(header: string, getValue: (p: T) => string) {
    super(header, getValue);
  }

  override render(td: HTMLElement, row: T) {
    const code = td.ownerDocument.createElement('code');
    code.innerText = this.getValue(row);

    const pre = td.ownerDocument.createElement('pre');
    pre.appendChild(code);

    td.appendChild(pre);
  }
}

function renderDL<T>(td: HTMLElement, row: T, cols: Array<Column<T>>) {
  const dl = td.ownerDocument.createElement('dl');

  cols.forEach(col => {
    const dt = td.ownerDocument.createElement('dt');
    col.renderHeader(dt);
    dl.appendChild(dt);

    const dd = td.ownerDocument.createElement('dd');
    col.render(dd, row);
    dl.appendChild(dd);
  });

  td.appendChild(dl);
}

function renderA(td: HTMLElement, text: string, href: string) {
  const a = td.ownerDocument.createElement('a');
  a.href = href;
  a.target = '_blank';
  a.innerText = text;
  td.appendChild(a);
}

class LogMetadataColumn implements Column<Log> {
  compare = null;

  renderHeader(th: HTMLElement) {
    th.innerText = 'Metadata';
  }

  render(td: HTMLElement, row: Log) {
    row.renderMetadata(td);
  }
}

class LogDescriptionColumn implements Column<Log> {
  compare = null;

  renderHeader(th: HTMLElement) {
    th.innerText = 'Description';
  }

  render(td: HTMLElement, row: Log) {
    row.renderDescription(td);
  }
}

const debugPathPattern: RegExp =
    /(?<=\/\.well-known\/attribution-reporting\/)debug(?=\/)/;

class ReportUrlColumn extends ValueColumn<Report, string> {
  constructor() {
    super('Report URL', (e) => e.reportUrl);
  }

  override render(td: HTMLElement, row: Report) {
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

class Selectable {
  input: HTMLInputElement;

  constructor() {
    this.input = document.createElement('input');
    this.input.type = 'checkbox';
  }
}

class SelectionColumn<T extends Selectable> implements Column<T> {
  compare: ((a: T, b: T) => number)|null;
  model: TableModel<T>;
  selectAll: HTMLInputElement;
  listener: () => void;
  selectionChangedListeners: Set<(param: boolean) => void>;

  constructor(model: TableModel<T>) {
    this.compare = null;
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
    this.selectionChangedListeners = new Set();
  }

  render(td: HTMLElement, row: T) {
    td.appendChild(row.input);
  }

  renderHeader(th: HTMLElement) {
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

  notifySelectionChanged(anySelected: boolean) {
    this.selectionChangedListeners.forEach((f) => f(anySelected));
  }
}

class Source {
  sourceEventId: bigint;
  sourceOrigin: string;
  attributionDestination: string;
  reportingOrigin: string;
  sourceTime: Date;
  expiryTime: Date;
  sourceType: string;
  filterData: string;
  aggregationKeys: string;
  debugKey: string;
  dedupKeys: string;
  priority: bigint;
  status: string;
  aggregatableBudgetConsumed: bigint;
  aggregatableDedupKeys: string;

  constructor(mojo: WebUISource) {
    this.sourceEventId = mojo.sourceEventId;
    this.sourceOrigin = originToText(mojo.sourceOrigin);
    this.attributionDestination = mojo.attributionDestination;
    this.reportingOrigin = originToText(mojo.reportingOrigin);
    this.sourceTime = new Date(mojo.sourceTime);
    this.expiryTime = new Date(mojo.expiryTime);
    this.sourceType = sourceTypeToText(mojo.sourceType);
    this.priority = mojo.priority;
    this.filterData = JSON.stringify(mojo.filterData, null, ' ');
    this.aggregationKeys =
        JSON.stringify(mojo.aggregationKeys, bigintReplacer, ' ');
    this.debugKey = mojo.debugKey ? mojo.debugKey.value.toString() : '';
    this.dedupKeys = mojo.dedupKeys.join(', ');
    this.aggregatableBudgetConsumed = mojo.aggregatableBudgetConsumed;
    this.aggregatableDedupKeys = mojo.aggregatableDedupKeys.join(', ');
    this.status = attributabilityToText(mojo.attributability);
  }
}

class SourceTableModel extends TableModel<Source> {
  storedSources: Source[] = [];
  unstoredSources: Source[] = [];

  constructor() {
    super();

    this.cols = [
      new ValueColumn<Source, bigint>(
          'Source Event ID', (e) => e.sourceEventId),
      new ValueColumn<Source, string>('Status', (e) => e.status),
      new ValueColumn<Source, string>('Source Origin', (e) => e.sourceOrigin),
      new ValueColumn<Source, string>(
          'Destination', (e) => e.attributionDestination),
      new ValueColumn<Source, string>('Report To', (e) => e.reportingOrigin),
      new DateColumn<Source>('Source Registration Time', (e) => e.sourceTime),
      new DateColumn<Source>('Expiry Time', (e) => e.expiryTime),
      new ValueColumn<Source, string>('Source Type', (e) => e.sourceType),
      new ValueColumn<Source, bigint>('Priority', (e) => e.priority),
      new CodeColumn<Source>('Filter Data', (e) => e.filterData),
      new CodeColumn<Source>('Aggregation Keys', (e) => e.aggregationKeys),
      new ValueColumn<Source, string>(
          'Aggregatable Budget Consumed',
          (e) => `${e.aggregatableBudgetConsumed} / ${BUDGET_PER_SOURCE}`),
      new ValueColumn<Source, string>('Debug Key', (e) => e.debugKey),
      new ValueColumn<Source, string>('Dedup Keys', (e) => e.dedupKeys),
      new ValueColumn<Source, string>(
          'Aggregatable Dedup Keys', (e) => e.aggregatableDedupKeys),
    ];

    this.emptyRowText = 'No sources.';

    // Sort by source registration time by default.
    this.sortIdx = 5;
  }

  override getRows() {
    return this.unstoredSources.concat(this.storedSources);
  }

  setStoredSources(storedSources: Source[]) {
    this.storedSources = storedSources;
    this.notifyRowsChanged();
  }

  addUnstoredSource(source: Source) {
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
  triggerTime: Date;
  destinationOrigin: string;
  reportingOrigin: string;
  filters: string;
  notFilters: string;
  debugKey: string;
  eventTriggers: string;
  eventLevelStatus: string;
  aggregatableStatus: string;
  aggregatableTriggers: string;
  aggregatableValues: string;
  aggregatableDedupKey: string;

  constructor(mojo: WebUITrigger) {
    this.triggerTime = new Date(mojo.triggerTime);
    this.destinationOrigin = originToText(mojo.destinationOrigin);
    this.reportingOrigin = originToText(mojo.reportingOrigin);
    this.filters = JSON.stringify(mojo.filters, null, ' ');
    this.notFilters = JSON.stringify(mojo.notFilters, null, ' ');
    this.debugKey = mojo.debugKey ? mojo.debugKey.value.toString() : '';

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
        bigintReplacer, ' ');

    this.aggregatableTriggers = JSON.stringify(
        mojo.aggregatableTriggers.map((e) => {
          // Omit the filters and not filters if they are empty for brevity.
          return {
            'key_piece': e.keyPiece,
            'source_keys': e.sourceKeys,
            'filters': Object.entries(e.filters).length > 0 ? e.filters :
                                                              undefined,
            'not_filters': Object.entries(e.notFilters).length > 0 ?
                e.notFilters :
                undefined,
          };
        }),
        bigintReplacer, ' ');

    this.aggregatableValues =
        JSON.stringify(mojo.aggregatableValues, null, ' ');

    this.aggregatableDedupKey = mojo.aggregatableDedupKey ?
        mojo.aggregatableDedupKey.value.toString() : '';

    this.eventLevelStatus = triggerStatusToText(mojo.eventLevelStatus);
    this.aggregatableStatus = triggerStatusToText(mojo.aggregatableStatus);
  }
}

class TriggerTableModel extends TableModel<Trigger> {
  triggers: Trigger[] = [];

  constructor() {
    super();

    this.cols = [
      new DateColumn<Trigger>('Trigger Time', (e) => e.triggerTime),
      new ValueColumn<Trigger, string>(
          'Event-Level Status', (e) => e.eventLevelStatus),
      new ValueColumn<Trigger, string>(
          'Aggregatable Status', (e) => e.aggregatableStatus),
      new ValueColumn<Trigger, string>(
          'Destination', (e) => e.destinationOrigin),
      new ValueColumn<Trigger, string>('Report To', (e) => e.reportingOrigin),
      new ValueColumn<Trigger, string>('Debug Key', (e) => e.debugKey),
      new CodeColumn<Trigger>('Filters', (e) => e.filters),
      new CodeColumn<Trigger>('Negated Filters', (e) => e.notFilters),
      new CodeColumn<Trigger>('Event Triggers', (e) => e.eventTriggers),
      new CodeColumn<Trigger>(
          'Aggregatable Triggers', (e) => e.aggregatableTriggers),
      new CodeColumn<Trigger>(
          'Aggregatable Values', (e) => e.aggregatableValues),
      new ValueColumn<Trigger, string>(
          'Aggregatable Dedup Key', (e) => e.aggregatableDedupKey),
    ];

    this.emptyRowText = 'No triggers.';

    // Sort by trigger time by default.
    this.sortIdx = 0;
  }

  override getRows() {
    return this.triggers;
  }

  addTrigger(trigger: Trigger) {
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
  id: ReportID;
  reportBody: string;
  reportUrl: string;
  triggerTime: Date;
  reportTime: Date;
  isDebug: boolean;
  status: string;
  httpResponseCode?: number;

  constructor(mojo: WebUIReport) {
    super();

    this.id = mojo.id;
    this.reportBody = mojo.reportBody;
    this.reportUrl = mojo.reportUrl.url;
    this.triggerTime = new Date(mojo.triggerTime);
    this.reportTime = new Date(mojo.reportTime);

    // Only pending reports are selectable.
    if (mojo.status.pending === undefined) {
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
      this.status = `Replaced by higher-priority report: ${
          mojo.status.replacedByHigherPriorityReport}`;
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

class EventLevelReport extends Report {
  reportPriority: bigint;
  attributedTruthfully: boolean;

  constructor(mojo: WebUIReport) {
    super(mojo);

    this.reportPriority = mojo.data.eventLevelData!.priority;
    this.attributedTruthfully = mojo.data.eventLevelData!.attributedTruthfully;
  }
}

class AggregatableAttributionReport extends Report {
  contributions: string;

  constructor(mojo: WebUIReport) {
    super(mojo);

    this.contributions = JSON.stringify(
        mojo.data.aggregatableAttributionData!.contributions, bigintReplacer,
        ' ');
  }
}

class ReportTableModel extends TableModel<Report> {
  showDebugReportsCheckbox: HTMLInputElement;
  hiddenDebugReportsSpan: HTMLSpanElement;
  sendReportsButton: HTMLButtonElement;
  selectionColumn: SelectionColumn<Report>;
  sentOrDroppedReports: Report[] = [];
  storedReports: Report[] = [];
  debugReports: Report[] = [];

  constructor(
      showDebugReportsContainer: HTMLElement,
      sendReportsButton: HTMLButtonElement) {
    super();

    const showDebugReportsCheckbox =
        showDebugReportsContainer.querySelector<HTMLInputElement>(
            'input[type="checkbox"]');
    assert(showDebugReportsCheckbox);
    this.showDebugReportsCheckbox = showDebugReportsCheckbox;

    const hiddenDebugReportsSpan =
        showDebugReportsContainer.querySelector('span');
    assert(hiddenDebugReportsSpan);
    this.hiddenDebugReportsSpan = hiddenDebugReportsSpan;

    this.sendReportsButton = sendReportsButton;

    this.selectionColumn = new SelectionColumn(this);

    this.emptyRowText = 'No sent or pending reports.';

    this.showDebugReportsCheckbox.addEventListener(
        'input', () => this.notifyRowsChanged());

    this.sendReportsButton.addEventListener('click', () => this.sendReports_());
    this.selectionColumn.selectionChangedListeners.add(
        (anySelected: boolean) => {
          this.sendReportsButton.disabled = !anySelected;
        });

    this.rowsChangedListeners.add(() => this.updateHiddenDebugReportsSpan_());
  }

  override styleRow(tr: HTMLElement, report: Report) {
    tr.classList.toggle(
        'http-error',
        report.httpResponseCode !== undefined &&
            (report.httpResponseCode < 200 || report.httpResponseCode >= 400));
  }

  override getRows() {
    let rows = this.sentOrDroppedReports.concat(this.storedReports);
    if (this.showDebugReportsCheckbox.checked) {
      rows = rows.concat(this.debugReports);
    }
    return rows;
  }

  setStoredReports(storedReports: Report[]) {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  addSentOrDroppedReport(report: Report) {
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

  private updateHiddenDebugReportsSpan_() {
    this.hiddenDebugReportsSpan.innerText =
        this.showDebugReportsCheckbox.checked ?
        '' :
        ` (${this.debugReports.length} hidden)`;
  }

  /**
   * Sends all selected reports.
   * Disables the button while the reports are still being sent.
   * Observer.onReportsChanged and Observer.onSourcesChanged will be called
   * automatically as reports are deleted, so there's no need to manually
   * refresh the data on completion.
   */
  private sendReports_() {
    const ids: ReportID[] = [];
    this.storedReports.forEach((report) => {
      if (!report.input.disabled && report.input.checked) {
        ids.push(report.id);
      }
    });

    if (ids.length === 0) {
      return;
    }

    const previousText = this.sendReportsButton.innerText;

    this.sendReportsButton.disabled = true;
    this.sendReportsButton.innerText = 'Sending...';

    assert(pageHandler);
    pageHandler.sendReports(ids).then(() => {
      this.sendReportsButton.innerText = previousText;
    });
  }
}

class EventLevelReportTableModel extends ReportTableModel {
  constructor(
      showDebugReportsContainer: HTMLElement,
      sendReportsButton: HTMLButtonElement) {
    super(showDebugReportsContainer, sendReportsButton);

    this.cols = [
      this.selectionColumn,
      new CodeColumn<Report>('Report Body', (e) => e.reportBody),
      new ValueColumn<Report, string>('Status', (e) => e.status),
      new ReportUrlColumn(),
      new DateColumn<Report>('Trigger Time', (e) => e.triggerTime),
      new DateColumn<Report>('Report Time', (e) => e.reportTime),
      new ValueColumn<Report, bigint>(
          'Report Priority', (e) => (e as EventLevelReport).reportPriority),
      new ValueColumn<Report, string>(
          'Randomized Report',
          (e) => (e as EventLevelReport).attributedTruthfully ? 'no' : 'yes'),
    ];

    // Sort by report time by default.
    this.sortIdx = 5;
  }
}

class AggregatableAttributionReportTableModel extends ReportTableModel {
  constructor(
      showDebugReportsContainer: HTMLElement,
      sendReportsButton: HTMLButtonElement) {
    super(showDebugReportsContainer, sendReportsButton);

    this.cols = [
      this.selectionColumn,
      new CodeColumn<Report>('Report Body', (e) => e.reportBody),
      new ValueColumn<Report, string>('Status', (e) => e.status),
      new ReportUrlColumn(),
      new DateColumn<Report>('Trigger Time', (e) => e.triggerTime),
      new DateColumn<Report>('Report Time', (e) => e.reportTime),
      new CodeColumn<Report>(
          'Histograms',
          (e) => (e as AggregatableAttributionReport).contributions),
    ];

    // Sort by report time by default.
    this.sortIdx = 5;
  }
}

abstract class Log {
  readonly timestamp: Date;
  readonly reportTo: string;

  constructor(mojo: {time: number, reportingOrigin: Origin}) {
    this.timestamp = new Date(mojo.time);
    this.reportTo = originToText(mojo.reportingOrigin);
  }

  abstract renderDescription(td: HTMLElement): void;

  abstract renderMetadata(td: HTMLElement): void;
}

const CLEARED_DEBUG_KEY_COLS: Array<Column<ClearedDebugKeyLog>> = [
  new ValueColumn<ClearedDebugKeyLog, string>(
      'Cleared Debug Key', e => e.clearedDebugKey),
  new ValueColumn<ClearedDebugKeyLog, string>('From', e => e.clearedFrom),
  new ValueColumn<ClearedDebugKeyLog, string>('Report To', e => e.reportTo),
];

class ClearedDebugKeyLog extends Log {
  readonly clearedFrom: string;
  readonly clearedDebugKey: string;

  constructor(mojo: ClearedDebugKey) {
    super(mojo);

    this.clearedDebugKey = `${mojo.clearedDebugKey.value}`;

    switch (mojo.clearedFrom) {
      case (ClearedDebugKey_Type.kSource):
        this.clearedFrom = 'Source';
        break;
      case (ClearedDebugKey_Type.kTrigger):
        this.clearedFrom = 'Trigger';
        break;
      default:
        this.clearedFrom = 'Unknown type';
        break;
    }
  }

  renderDescription(td: HTMLElement): void {
    renderA(
        td,
        'Cleared Debug Key',
        'https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#optional-extended-debugging-reports',
    );
  }

  renderMetadata(td: HTMLElement) {
    renderDL(td, this, CLEARED_DEBUG_KEY_COLS);
  }
}

const FAILED_SOURCE_REGISTRATION_COLS:
    Array<Column<FailedSourceRegistrationLog>> = [
      new ValueColumn<FailedSourceRegistrationLog, string>(
          'Failure Reason', e => e.failureReason),
      new ValueColumn<FailedSourceRegistrationLog, string>(
          'Report To', e => e.reportTo),
      new CodeColumn<FailedSourceRegistrationLog>(
          'Attribution-Reporting-Register-Source Header', e => e.headerValue),
    ];

class FailedSourceRegistrationLog extends Log {
  readonly failureReason: string;
  readonly headerValue: string;

  constructor(mojo: FailedSourceRegistration) {
    super(mojo);

    switch (mojo.error) {
      case SourceRegistrationError.kInvalidJson:
        this.failureReason = 'invalid JSON';
        break;
      case SourceRegistrationError.kRootWrongType:
        this.failureReason =
            'root JSON value has wrong type (must be a dictionary)';
        break;
      case SourceRegistrationError.kDestinationMissing:
        this.failureReason = 'destination missing';
        break;
      case SourceRegistrationError.kDestinationWrongType:
        this.failureReason = 'destination has wrong type (must be a string)';
        break;
      case SourceRegistrationError.kDestinationUntrustworthy:
        this.failureReason = 'destination not potentially trustworthy';
        break;
      case SourceRegistrationError.kFilterDataWrongType:
        this.failureReason =
            'filter_data has wrong type (must be a dictionary)';
        break;
      case SourceRegistrationError.kFilterDataTooManyKeys:
        this.failureReason = 'filter_data has too many keys';
        break;
      case SourceRegistrationError.kFilterDataHasSourceTypeKey:
        this.failureReason = 'filter_data must not have a source_type key';
        break;
      case SourceRegistrationError.kFilterDataKeyTooLong:
        this.failureReason = 'filter_data key too long';
        break;
      case SourceRegistrationError.kFilterDataListWrongType:
        this.failureReason =
            'filter_data value has wrong type (must be a list)';
        break;
      case SourceRegistrationError.kFilterDataListTooLong:
        this.failureReason = 'filter_data list too long';
        break;
      case SourceRegistrationError.kFilterDataValueWrongType:
        this.failureReason =
            'filter_data list value has wrong type (must be a string)';
        break;
      case SourceRegistrationError.kFilterDataValueTooLong:
        this.failureReason = 'filter_data list value too long';
        break;
      case SourceRegistrationError.kAggregationKeysWrongType:
        this.failureReason =
            'aggregation_keys has wrong type (must be a dictionary)';
        break;
      case SourceRegistrationError.kAggregationKeysTooManyKeys:
        this.failureReason = 'aggregation_keys has too many keys';
        break;
      case SourceRegistrationError.kAggregationKeysKeyTooLong:
        this.failureReason = 'aggregation_keys key too long';
        break;
      case SourceRegistrationError.kAggregationKeysValueWrongType:
        this.failureReason =
            'aggregation_keys value has wrong type (must be a string)';
        break;
      case SourceRegistrationError.kAggregationKeysValueWrongFormat:
        this.failureReason =
            'aggregation_keys value must be a base-16 integer starting with 0x';
        break;
      default:
        this.failureReason = 'unknown error';
        break;
    }

    this.headerValue = mojo.headerValue;
  }

  renderDescription(td: HTMLElement) {
    renderA(
        td,
        'Failed Source Registration',
        'https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#registering-attribution-sources',
    );
  }

  renderMetadata(td: HTMLElement) {
    renderDL(td, this, FAILED_SOURCE_REGISTRATION_COLS);
  }
}

class LogTableModel extends TableModel<Log> {
  logs: Log[] = [];

  constructor() {
    super();

    this.cols = [
      new DateColumn<Log>('Timestamp', (e) => e.timestamp),
      new LogDescriptionColumn(),
      new LogMetadataColumn(),
    ];

    this.emptyRowText = 'No logs.';

    // Sort by time by default.
    this.sortIdx = 0;
  }

  override getRows() {
    return this.logs;
  }

  addLog(log: Log) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.logs.length >= 1000) {
      this.logs = [];
    }

    this.logs.push(log);
    this.notifyRowsChanged();
  }

  clear() {
    this.logs = [];
    this.notifyRowsChanged();
  }
}

/**
 * Reference to the backend providing all the data.
 */
let pageHandler: AttributionInternalsHandlerRemote|null = null;

let sourceTableModel: SourceTableModel|null = null;

let triggerTableModel: TriggerTableModel|null = null;

let eventLevelReportTableModel: EventLevelReportTableModel|null = null;

let logTableModel: LogTableModel|null = null;

let aggregatableAttributionReportTableModel:
    AggregatableAttributionReportTableModel|null = null;

/**
 * Converts a mojo origin into a user-readable string, omitting default ports.
 * @param origin Origin to convert
 */
function originToText(origin: Origin): string {
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
 * @param sourceType Source type to convert
 */
function sourceTypeToText(sourceType: SourceType): string {
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
 * @param attributability Attributability to convert
 */
function attributabilityToText(attributability: WebUISource_Attributability):
    string {
  switch (attributability) {
    case WebUISource_Attributability.kAttributable:
      return 'Attributable';
    case WebUISource_Attributability.kNoised:
      return 'Unattributable: noised';
    case WebUISource_Attributability.kReachedEventLevelAttributionLimit:
      return 'Attributable: reached event-level attribution limit';
    case WebUISource_Attributability.kInternalError:
      return 'Rejected: internal error';
    case WebUISource_Attributability.kInsufficientSourceCapacity:
      return 'Rejected: insufficient source capacity';
    case WebUISource_Attributability.kInsufficientUniqueDestinationCapacity:
      return 'Rejected: insufficient unique destination capacity';
    case WebUISource_Attributability.kExcessiveReportingOrigins:
      return 'Rejected: excessive reporting origins';
    case WebUISource_Attributability.kProhibitedByBrowserPolicy:
      return 'Rejected: prohibited by browser policy';
    default:
      return attributability.toString();
  }
}

function triggerStatusToText(status: WebUITrigger_Status): string {
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
    case WebUITrigger_Status.kNoMatchingConfigurations:
      return 'Rejected: no matching event-level configurations';
    case WebUITrigger_Status.kExcessiveEventLevelReports:
      return 'Failure: Excessive event-level reports';
    default:
      return status.toString();
  }
}

/**
 * Fetch all sources, pending reports, and sent reports from the
 * backend and populate the tables. Also update measurement enabled status.
 */
function updatePageData() {
  assert(pageHandler);
  // Get the feature status for Attribution Reporting and populate it.
  pageHandler.isAttributionReportingEnabled().then((response) => {
    const featureStatusContent =
        document.querySelector<HTMLElement>('#feature-status-content');
    assert(featureStatusContent);
    featureStatusContent.innerText = response.enabled ? 'enabled' : 'disabled';
    featureStatusContent.classList.toggle('disabled', !response.enabled);

    const debugModeContent =
        document.querySelector<HTMLElement>('#debug-mode-content');
    assert(debugModeContent);
    const html = getTrustedHTML`The #attribution-reporting-debug-mode flag is
 <strong>enabled</strong>, reports are sent immediately and never pending.`;
    debugModeContent.innerHTML = html as unknown as string;

    if (!response.debugMode) {
      debugModeContent.innerText = '';
    }
  });

  updateSources();
  updateReports(ReportType.kEventLevel);
  updateReports(ReportType.kAggregatableAttribution);
}

function updateSources() {
  assert(pageHandler);
  pageHandler.getActiveSources().then((response) => {
    assert(sourceTableModel);
    sourceTableModel.setStoredSources(
        response.sources.map((mojo) => new Source(mojo)));
  });
}

function updateReports(reportType: ReportType) {
  assert(pageHandler);
  pageHandler.getReports(reportType).then((response) => {
    switch (reportType) {
      case ReportType.kEventLevel:
        assert(eventLevelReportTableModel);
        eventLevelReportTableModel.setStoredReports(
            response.reports
                .filter((mojo) => mojo.data.eventLevelData !== undefined)
                .map((mojo) => new EventLevelReport(mojo)));
        break;
      case ReportType.kAggregatableAttribution:
        assert(aggregatableAttributionReportTableModel);
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
  assert(sourceTableModel);
  sourceTableModel.clear();
  assert(triggerTableModel);
  triggerTableModel.clear();
  assert(eventLevelReportTableModel);
  eventLevelReportTableModel.clear();
  assert(aggregatableAttributionReportTableModel);
  aggregatableAttributionReportTableModel.clear();
  assert(logTableModel);
  logTableModel.clear();
  assert(pageHandler);
  pageHandler.clearStorage();
}

function addSentOrDroppedReport(mojo: WebUIReport) {
  if (mojo.data.eventLevelData !== undefined) {
    assert(eventLevelReportTableModel);
    eventLevelReportTableModel.addSentOrDroppedReport(
        new EventLevelReport(mojo));
  } else {
    assert(aggregatableAttributionReportTableModel);
    aggregatableAttributionReportTableModel.addSentOrDroppedReport(
        new AggregatableAttributionReport(mojo));
  }
}

class Observer implements ObserverInterface {
  onSourcesChanged() {
    updateSources();
  }

  onReportsChanged(reportType: ReportType) {
    updateReports(reportType);
  }

  onSourceRejected(mojo: WebUISource) {
    assert(sourceTableModel);
    sourceTableModel.addUnstoredSource(new Source(mojo));
  }

  onReportSent(mojo: WebUIReport) {
    addSentOrDroppedReport(mojo);
  }

  onReportDropped(mojo: WebUIReport) {
    addSentOrDroppedReport(mojo);
  }

  onTriggerHandled(mojo: WebUITrigger) {
    assert(triggerTableModel);
    triggerTableModel.addTrigger(new Trigger(mojo));
  }

  onFailedSourceRegistration(mojo: FailedSourceRegistration) {
    assert(logTableModel);
    logTableModel.addLog(new FailedSourceRegistrationLog(mojo));
  }

  onDebugKeyCleared(mojo: ClearedDebugKey) {
    assert(logTableModel);
    logTableModel.addLog(new ClearedDebugKeyLog(mojo));
  }
}

function installUnreadIndicator(model: TableModel<any>, tab: HTMLElement|null) {
  assert(tab);

  model.rowsChangedListeners.add(() => {
    if (!tab.hasAttribute('selected')) {
      tab.classList.add('unread');
    }
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup the mojo interface.
  pageHandler = AttributionInternalsHandler.getRemote();

  sourceTableModel = new SourceTableModel();
  triggerTableModel = new TriggerTableModel();
  const showDebugReports =
      document.querySelector<HTMLButtonElement>('#show-debug-event-reports');
  assert(showDebugReports);
  const sendReports =
      document.querySelector<HTMLButtonElement>('#send-reports');
  assert(sendReports);
  eventLevelReportTableModel =
      new EventLevelReportTableModel(showDebugReports, sendReports);
  const showDebugAggregatableReports =
      document.querySelector<HTMLElement>('#show-debug-aggregatable-reports');
  assert(showDebugAggregatableReports);
  const sendAggregatableReports =
      document.querySelector<HTMLButtonElement>('#send-aggregatable-reports');
  assert(sendAggregatableReports);
  aggregatableAttributionReportTableModel =
      new AggregatableAttributionReportTableModel(
          showDebugAggregatableReports, sendAggregatableReports);
  logTableModel = new LogTableModel();

  const tabBox = document.querySelector('cr-tab-box');
  assert(tabBox);
  tabBox.addEventListener('selected-index-change', e => {
    const tabs = document.querySelectorAll<HTMLElement>('div[slot=\'tab\']');
    tabs[(e as CustomEvent<number>).detail]!.classList.remove('unread');
  });

  installUnreadIndicator(
      sourceTableModel, document.querySelector<HTMLElement>('#sources-tab'));
  installUnreadIndicator(
      triggerTableModel, document.querySelector<HTMLElement>('#triggers-tab'));
  installUnreadIndicator(
      eventLevelReportTableModel,
      document.querySelector<HTMLElement>('#event-level-reports-tab'));
  installUnreadIndicator(
      aggregatableAttributionReportTableModel,
      document.querySelector<HTMLElement>('#aggregatable-reports-tab'));
  installUnreadIndicator(
      logTableModel, document.querySelector<HTMLElement>('#logs-tab'));

  const refresh = document.querySelector('#refresh');
  assert(refresh);
  refresh.addEventListener('click', updatePageData);
  const clearData = document.querySelector('#clear-data');
  assert(clearData);
  clearData.addEventListener('click', clearStorage);

  const sourceTable =
      document.querySelector<AttributionInternalsTableElement<Source>>(
          '#sourceTable');
  assert(sourceTable);
  sourceTable.setModel(sourceTableModel!);
  const triggerTable =
      document.querySelector<AttributionInternalsTableElement<Trigger>>(
          '#triggerTable');
  assert(triggerTable);
  triggerTable.setModel(triggerTableModel!);
  const reportTable =
      document.querySelector<AttributionInternalsTableElement<Report>>(
          '#reportTable');
  assert(reportTable);
  reportTable.setModel(eventLevelReportTableModel!);
  const aggregatableReportTable =
      document.querySelector<AttributionInternalsTableElement<Report>>(
          '#aggregatableReportTable');
  assert(aggregatableReportTable);
  aggregatableReportTable.setModel(aggregatableAttributionReportTableModel!);
  const logTable =
      document.querySelector<AttributionInternalsTableElement<Log>>(
          '#logTable');
  assert(logTable);
  logTable.setModel(logTableModel);

  tabBox.hidden = false;

  const receiver = new ObserverReceiver(new Observer());
  assert(pageHandler);
  pageHandler.addObserver(receiver.$.bindNewPipeAndPassRemote());

  updatePageData();
});

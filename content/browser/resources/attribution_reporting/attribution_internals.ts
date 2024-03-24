// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './attribution_internals_table.js';

import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {AggregatableResult} from './aggregatable_result.mojom-webui.js';
import type {TriggerVerification} from './attribution.mojom-webui.js';
import {AttributionSupport} from './attribution.mojom-webui.js';
import type {HandlerInterface, ObserverInterface, ReportID, WebUIDebugReport, WebUIOsRegistration, WebUIRegistration, WebUIReport, WebUISource, WebUISourceRegistration, WebUITrigger} from './attribution_internals.mojom-webui.js';
import {Factory, HandlerRemote, ObserverReceiver, WebUISource_Attributability} from './attribution_internals.mojom-webui.js';
import type {AttributionInternalsTableElement} from './attribution_internals_table.js';
import {OsRegistrationResult, RegistrationType} from './attribution_reporting.mojom-webui.js';
import {EventLevelResult} from './event_level_result.mojom-webui.js';
import {SourceType} from './source_type.mojom-webui.js';
import {StoreSourceResult} from './store_source_result.mojom-webui.js';
import type {Column} from './table_model.js';
import {ArrayTableModel, TableModel} from './table_model.js';
import {TriggerDataMatching} from './trigger_data_matching.mojom-webui.js';

// If kAttributionAggregatableBudgetPerSource changes, update this value
const BUDGET_PER_SOURCE = 65536;

type Comparable = bigint|number|string|boolean|Date;

function compareDefault<T extends Comparable>(a: T, b: T): number {
  if (a < b) {
    return -1;
  }
  if (a > b) {
    return 1;
  }
  return 0;
}

type CompareFunc<V> = (a: V, b: V) => number;

function undefinedFirst<V>(f: CompareFunc<V>): CompareFunc<V|undefined> {
  return (a: V|undefined, b: V|undefined): number => {
    if (a === undefined && b === undefined) {
      return 0;
    }
    if (a === undefined) {
      return -1;
    }
    if (b === undefined) {
      return 1;
    }
    return f(a, b);
  };
}

function compareLexicographic<V>(f: CompareFunc<V>): CompareFunc<V[]> {
  return (a: V[], b: V[]): number => {
    for (let i = 0; i < a.length && i < b.length; ++i) {
      const r = f(a[i]!, b[i]!);
      if (r !== 0) {
        return r;
      }
    }
    return compareDefault(a.length, b.length);
  };
}

function bigintReplacer(_key: string, value: any): any {
  return typeof value === 'bigint' ? value.toString() : value;
}

type RenderFunc<V> = (e: HTMLElement, v: V) => void;

interface Valuable<V> {
  readonly compare?: CompareFunc<V>;
  readonly render: RenderFunc<V>;
}

function allowingUndefined<V>({render, compare}: Valuable<V>):
    Valuable<V|undefined> {
  return {
    compare: compare ? undefinedFirst(compare) : undefined,
    render: (td: HTMLElement, v: V|undefined) => {
      if (v !== undefined) {
        render(td, v);
      }
    },
  };
}

class ValueColumn<T> implements Column<T> {
  static of<T, K extends keyof T>(
      header: string, key: K,
      {render, compare}: Valuable<T[K]>): ValueColumn<T> {
    return new ValueColumn<T>(
        header, (td, row) => render(td, row[key]),
        compare ? (a, b) => compare(a[key], b[key]) : undefined);
  }

  constructor(
      private readonly header: string, readonly render: RenderFunc<T>,
      readonly compare?: CompareFunc<T>) {}

  renderHeader(th: HTMLElement): void {
    th.innerText = this.header;
  }
}

const asDate: Valuable<Date> = {
  compare: compareDefault,
  render: (td: HTMLElement, v: Date) => {
    const time = td.ownerDocument.createElement('time');
    time.dateTime = v.toISOString();
    time.innerText = v.toLocaleString();
    td.append(time);
  },
};

const numberClass: string = 'number';

const asNumber: Valuable<bigint|number> = {
  compare: compareDefault,
  render: (td: HTMLElement, v: bigint|number) => {
    td.classList.add(numberClass);
    td.innerText = v.toString();
  },
};

function asCustomNumber<V extends bigint|number>(fmt: (v: V) => string):
    Valuable<V> {
  return {
    compare: compareDefault,
    render: (td: HTMLElement, v: V) => {
      td.classList.add(numberClass);
      td.innerText = fmt(v);
    },
  };
}

const asStringOrBool: Valuable<string|boolean> = {
  compare: compareDefault,
  render: (td: HTMLElement, v: string|boolean) => td.innerText = v.toString(),
};

const asCode: Valuable<string> = {
  render: (td: HTMLElement, v: string) => {
    const code = td.ownerDocument.createElement('code');
    code.innerText = v;

    const pre = td.ownerDocument.createElement('pre');
    pre.append(code);

    td.append(pre);
  },
};

function asList<V>({render, compare}: Valuable<V>): Valuable<V[]> {
  return {
    compare: compare ? compareLexicographic(compare) : undefined,
    render: (td: HTMLElement, vs: V[]) => {
      if (vs.length === 0) {
        return;
      }

      const ul = td.ownerDocument.createElement('ul');

      for (const v of vs) {
        const li = td.ownerDocument.createElement('li');
        render(li, v);
        ul.append(li);
      }

      td.append(ul);
    },
  };
}

function renderDL<T>(td: HTMLElement, row: T, cols: Iterable<Column<T>>): void {
  const dl = td.ownerDocument.createElement('dl');

  for (const col of cols) {
    const dt = td.ownerDocument.createElement('dt');
    col.renderHeader(dt);

    const dd = td.ownerDocument.createElement('dd');
    col.render(dd, row);

    dl.append(dt, dd);
  }

  td.append(dl);
}

function renderUrl(
    td: HTMLElement, url: string,
    renderAnchor: RenderFunc<string> = (td, v) => td.innerText = v): void {
  const a = td.ownerDocument.createElement('a');
  a.target = '_blank';
  a.href = url;
  renderAnchor(a, url);
  td.append(a);
}

const asUrl: Valuable<string> = {
  compare: compareDefault,
  render: renderUrl,
};

const debugPathPattern: RegExp =
    /(?<=\/\.well-known\/attribution-reporting\/)debug(?=\/)/;

const reportUrlColumn: Column<Report> =
    ValueColumn.of('Report URL', 'reportUrl', {
      compare: compareDefault,
      render: (td: HTMLElement, url: string) => renderUrl(
          td, url,
          (a, url) => {
            const [pre, post] = url.split(debugPathPattern, 2);
            if (pre === undefined || post === undefined) {
              a.innerText = url;
              return;
            }

            const span = a.ownerDocument.createElement('span');
            span.classList.add('debug-url');
            span.innerText = 'debug';

            a.append(pre, span, post);
          }),
    });

class Selectable {
  readonly input: HTMLInputElement;

  constructor() {
    this.input = document.createElement('input');
    this.input.type = 'checkbox';
    this.input.title = 'Select';
  }
}

class SelectionColumn<T extends Selectable> implements Column<T> {
  private readonly selectAll: HTMLInputElement;
  private readonly listener: () => void;
  readonly selectionChangedListeners: Set<(param: boolean) => void> = new Set();

  constructor(private readonly model: TableModel<T>) {
    this.selectAll = document.createElement('input');
    this.selectAll.type = 'checkbox';
    this.selectAll.title = 'Select All';
    this.selectAll.addEventListener('input', () => {
      const checked = this.selectAll.checked;
      for (const row of this.model.getRows()) {
        if (!row.input.disabled) {
          row.input.checked = checked;
        }
      }
      this.notifySelectionChanged(checked);
    });

    this.listener = () => this.onChange();
    this.model.rowsChangedListeners.add(this.listener);
  }

  render(td: HTMLElement, row: T): void {
    td.append(row.input);
  }

  renderHeader(th: HTMLElement): void {
    th.append(this.selectAll);
  }

  onChange(): void {
    let anySelected = false;
    let anyUnselected = false;

    for (const row of this.model.getRows()) {
      // addEventListener deduplicates, so only one event will be fired per
      // input.
      row.input.addEventListener('input', this.listener);

      if (!row.input.disabled) {
        if (row.input.checked) {
          anySelected = true;
        } else {
          anyUnselected = true;
        }
      }
    }

    this.selectAll.disabled = !anySelected && !anyUnselected;
    this.selectAll.checked = anySelected && !anyUnselected;
    this.selectAll.indeterminate = anySelected && anyUnselected;

    this.notifySelectionChanged(anySelected);
  }

  notifySelectionChanged(anySelected: boolean): void {
    this.selectionChangedListeners.forEach((f) => f(anySelected));
  }
}

class Source {
  sourceEventId: bigint;
  sourceOrigin: string;
  destinations: string[];
  reportingOrigin: string;
  sourceTime: Date;
  expiryTime: Date;
  triggerSpecs: string;
  aggregatableReportWindowTime: Date;
  maxEventLevelReports: number;
  sourceType: string;
  filterData: string;
  aggregationKeys: string;
  debugKey?: bigint;
  dedupKeys: bigint[];
  priority: bigint;
  status: string;
  aggregatableBudgetConsumed: bigint;
  aggregatableDedupKeys: bigint[];
  triggerDataMatching: string;
  eventLevelEpsilon: number;
  debugCookieSet: boolean;

  constructor(mojo: WebUISource) {
    this.sourceEventId = mojo.sourceEventId;
    this.sourceOrigin = originToText(mojo.sourceOrigin);
    this.destinations =
        mojo.destinations.destinations.map(d => originToText(d.siteAsOrigin))
            .sort(compareDefault);
    this.reportingOrigin = originToText(mojo.reportingOrigin);
    this.sourceTime = new Date(mojo.sourceTime);
    this.expiryTime = new Date(mojo.expiryTime);
    this.triggerSpecs = mojo.triggerSpecsJson;
    this.aggregatableReportWindowTime =
        new Date(mojo.aggregatableReportWindowTime);
    this.maxEventLevelReports = mojo.maxEventLevelReports;
    this.sourceType = sourceTypeText[mojo.sourceType];
    this.priority = mojo.priority;
    this.filterData = JSON.stringify(mojo.filterData.filterValues, null, ' ');
    this.aggregationKeys =
        JSON.stringify(mojo.aggregationKeys, bigintReplacer, ' ');
    // TODO(crbug.com/1442785): Workaround for undefined/null issue.
    this.debugKey =
        typeof mojo.debugKey === 'bigint' ? mojo.debugKey : undefined;
    this.dedupKeys = mojo.dedupKeys.sort(compareDefault);
    this.aggregatableBudgetConsumed = mojo.aggregatableBudgetConsumed;
    this.aggregatableDedupKeys =
        mojo.aggregatableDedupKeys.sort(compareDefault);
    this.triggerDataMatching =
        triggerDataMatchingText[mojo.triggerDataMatching];
    this.eventLevelEpsilon = mojo.eventLevelEpsilon;
    this.status = attributabilityText[mojo.attributability];
    this.debugCookieSet = mojo.debugCookieSet;
  }
}

class SourceTableModel extends ArrayTableModel<Source> {
  constructor() {
    super(
        [
          ValueColumn.of('Source Event ID', 'sourceEventId', asNumber),
          ValueColumn.of('Status', 'status', asStringOrBool),
          ValueColumn.of('Source Origin', 'sourceOrigin', asUrl),
          ValueColumn.of('Destinations', 'destinations', asList(asUrl)),
          ValueColumn.of('Reporting Origin', 'reportingOrigin', asUrl),
          ValueColumn.of('Registration Time', 'sourceTime', asDate),
          ValueColumn.of('Expiry Time', 'expiryTime', asDate),
          ValueColumn.of('Trigger Specs', 'triggerSpecs', asCode),
          ValueColumn.of(
              'Aggregatable Report Window Time', 'aggregatableReportWindowTime',
              asDate),
          ValueColumn.of(
              'Max Event Level Reports', 'maxEventLevelReports', asNumber),
          ValueColumn.of('Source Type', 'sourceType', asStringOrBool),
          ValueColumn.of('Priority', 'priority', asNumber),
          ValueColumn.of('Filter Data', 'filterData', asCode),
          ValueColumn.of('Aggregation Keys', 'aggregationKeys', asCode),
          ValueColumn.of(
              'Trigger Data Matching', 'triggerDataMatching', asStringOrBool),
          ValueColumn.of(
              'Event-Level Epsilon', 'eventLevelEpsilon',
              asCustomNumber((v: number) => v.toFixed(3))),
          ValueColumn.of(
              'Aggregatable Budget Consumed', 'aggregatableBudgetConsumed',
              asCustomNumber((v) => `${v} / ${BUDGET_PER_SOURCE}`)),
          ValueColumn.of('Debug Key', 'debugKey', allowingUndefined(asNumber)),
          ValueColumn.of('Debug Cookie Set', 'debugCookieSet', asStringOrBool),
          ValueColumn.of('Dedup Keys', 'dedupKeys', asList(asNumber)),
          ValueColumn.of(
              'Aggregatable Dedup Keys', 'aggregatableDedupKeys',
              asList(asNumber)),
        ],
        5,  // Sort by registration time by default.
        'No sources.',
    );
  }
}

class Registration {
  readonly time: Date;
  readonly contextOrigin: string;
  readonly reportingOrigin: string;
  readonly registrationJson: string;
  readonly clearedDebugKey?: bigint;

  constructor(mojo: WebUIRegistration) {
    this.time = new Date(mojo.time);
    this.contextOrigin = originToText(mojo.contextOrigin);
    this.reportingOrigin = originToText(mojo.reportingOrigin);
    this.registrationJson = mojo.registrationJson;
    // TODO(crbug.com/1442785): Workaround for undefined/null issue.
    this.clearedDebugKey = typeof mojo.clearedDebugKey === 'bigint' ?
        mojo.clearedDebugKey :
        undefined;
  }
}

class RegistrationTableModel<T extends Registration> extends
    ArrayTableModel<T> {
  constructor(contextOriginTitle: string, cols: Iterable<Column<T>>) {
    super(
        [
          ValueColumn.of('Time', 'time', asDate),
          ValueColumn.of(contextOriginTitle, 'contextOrigin', asUrl),
          ValueColumn.of('Reporting Origin', 'reportingOrigin', asUrl),
          ValueColumn.of('Registration JSON', 'registrationJson', asCode),
          ValueColumn.of(
              'Cleared Debug Key', 'clearedDebugKey',
              allowingUndefined(asNumber)),
          ...cols,
        ],
        0,  // Sort by time by default.
        'No registrations.',
    );
  }
}

class Trigger extends Registration {
  readonly eventLevelResult: string;
  readonly aggregatableResult: string;
  readonly verifications: TriggerVerification[];

  constructor(mojo: WebUITrigger) {
    super(mojo.registration);
    this.eventLevelResult = eventLevelResultText[mojo.eventLevelResult];
    this.aggregatableResult = aggregatableResultText[mojo.aggregatableResult];
    this.verifications = mojo.verifications;
  }
}

const VERIFICATION_COLS: ReadonlyArray<Column<TriggerVerification>> = [
  ValueColumn.of('Token', 'token', asStringOrBool),
  ValueColumn.of('Report ID', 'aggregatableReportId', asStringOrBool),
];

class ReportVerificationColumn implements Column<Trigger> {
  renderHeader(th: HTMLElement): void {
    th.innerText = 'Report Verification';
  }

  render(td: HTMLElement, row: Trigger): void {
    for (const verification of row.verifications) {
      renderDL(td, verification, VERIFICATION_COLS);
    }
  }
}

class TriggerTableModel extends RegistrationTableModel<Trigger> {
  constructor() {
    super('Destination', [
      ValueColumn.of('Event-Level Result', 'eventLevelResult', asStringOrBool),
      ValueColumn.of(
          'Aggregatable Result', 'aggregatableResult', asStringOrBool),
      new ReportVerificationColumn(),
    ]);
  }
}

class SourceRegistration extends Registration {
  readonly type: string;
  readonly status: string;

  constructor(mojo: WebUISourceRegistration) {
    super(mojo.registration);
    this.type = sourceTypeText[mojo.type];
    this.status = sourceRegistrationStatusText[mojo.status];
  }
}

class SourceRegistrationTableModel extends
    RegistrationTableModel<SourceRegistration> {
  constructor() {
    super('Source Origin', [
      ValueColumn.of('Type', 'type', asStringOrBool),
      ValueColumn.of('Status', 'status', asStringOrBool),
    ]);
  }
}

class Report extends Selectable {
  id: ReportID;
  reportBody: string;
  reportUrl: string;
  triggerTime: Date;
  reportTime: Date;
  status: string;
  sendFailed: boolean;

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

    this.sendFailed = false;

    if (mojo.status.sent !== undefined) {
      this.status = `Sent: HTTP ${mojo.status.sent}`;
      this.sendFailed = mojo.status.sent < 200 || mojo.status.sent >= 400;
    } else if (mojo.status.pending !== undefined) {
      this.status = 'Pending';
    } else if (mojo.status.replacedByHigherPriorityReport !== undefined) {
      this.status = `Replaced by higher-priority report: ${
          mojo.status.replacedByHigherPriorityReport}`;
    } else if (mojo.status.prohibitedByBrowserPolicy !== undefined) {
      this.status = 'Prohibited by browser policy';
    } else if (mojo.status.networkError !== undefined) {
      this.status = `Network error: ${mojo.status.networkError}`;
      this.sendFailed = true;
    } else if (mojo.status.failedToAssemble !== undefined) {
      this.status = 'Dropped due to assembly failure';
    } else {
      throw new Error('invalid ReportStatus union');
    }
  }

  isDebug(): boolean {
    return debugPathPattern.test(this.reportUrl);
  }
}

class EventLevelReport extends Report {
  reportPriority: bigint;
  randomizedReport: boolean;

  constructor(mojo: WebUIReport) {
    super(mojo);

    this.reportPriority = mojo.data.eventLevelData!.priority;
    this.randomizedReport = !mojo.data.eventLevelData!.attributedTruthfully;
  }
}

class AggregatableAttributionReport extends Report {
  contributions: string;
  verificationToken: string;
  aggregationCoordinator: string;
  isNullReport: boolean;

  constructor(mojo: WebUIReport) {
    super(mojo);

    this.contributions = JSON.stringify(
        mojo.data.aggregatableAttributionData!.contributions, bigintReplacer,
        ' ');

    this.verificationToken =
        mojo.data.aggregatableAttributionData!.verificationToken || '';

    this.aggregationCoordinator =
        mojo.data.aggregatableAttributionData!.aggregationCoordinator;

    this.isNullReport = mojo.data.aggregatableAttributionData!.isNullReport;
  }
}

class ReportTableModel<T extends Report> extends TableModel<T> {
  private readonly sendReportsButton: HTMLButtonElement;
  private readonly showDebugReportsCheckbox: HTMLInputElement;
  private readonly hiddenDebugReportsSpan: HTMLSpanElement;

  private sentOrDroppedReports: T[] = [];
  private storedReports: T[] = [];
  private debugReports: T[] = [];

  constructor(
      container: HTMLElement, private readonly handler: HandlerInterface,
      cols: Iterable<Column<T>>) {
    super(
        [
          ValueColumn.of('Status', 'status', asStringOrBool),
          reportUrlColumn,
          ValueColumn.of('Trigger Time', 'triggerTime', asDate),
          ValueColumn.of('Report Time', 'reportTime', asDate),
          ...cols,
          ValueColumn.of('Report Body', 'reportBody', asCode),
        ],
        4,  // Sort by report time by default; the extra column is added below
        'No sent or pending reports.',
    );

    // This can't be included in the super call above, as `this` can't be
    // accessed until after `super` returns.
    const selectionColumn = new SelectionColumn<T>(this);
    this.cols.unshift(selectionColumn);

    this.sendReportsButton = container.querySelector('button')!;

    this.showDebugReportsCheckbox =
        container.querySelector('input[type="checkbox"]')!;

    this.hiddenDebugReportsSpan = container.querySelector('span')!;

    this.showDebugReportsCheckbox.addEventListener(
        'input', () => this.notifyRowsChanged());

    this.sendReportsButton.addEventListener('click', () => this.sendReports_());
    selectionColumn.selectionChangedListeners.add((anySelected: boolean) => {
      this.sendReportsButton.disabled = !anySelected;
    });

    this.rowsChangedListeners.add(() => this.updateHiddenDebugReportsSpan_());
  }

  override styleRow(tr: HTMLElement, report: Report): void {
    tr.classList.toggle('send-error', report.sendFailed);
  }

  override empty(): boolean {
    return this.sentOrDroppedReports.length === 0 &&
        this.storedReports.length === 0 &&
        (!this.showDebugReportsCheckbox.checked ||
         this.debugReports.length === 0);
  }

  override getRows(): T[] {
    let rows = this.sentOrDroppedReports.concat(this.storedReports);
    if (this.showDebugReportsCheckbox.checked) {
      rows = rows.concat(this.debugReports);
    }
    return rows;
  }

  setStoredReports(storedReports: T[]): void {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  addSentOrDroppedReport(report: T): void {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.sentOrDroppedReports.length + this.debugReports.length >= 1000) {
      this.sentOrDroppedReports = [];
      this.debugReports = [];
    }

    if (report.isDebug()) {
      this.debugReports.push(report);
    } else {
      this.sentOrDroppedReports.push(report);
    }

    this.notifyRowsChanged();
  }

  clear(): void {
    this.storedReports = [];
    this.sentOrDroppedReports = [];
    this.debugReports = [];
    this.notifyRowsChanged();
  }

  private updateHiddenDebugReportsSpan_(): void {
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
  private sendReports_(): void {
    const ids: ReportID[] = [];
    for (const report of this.storedReports) {
      if (!report.input.disabled && report.input.checked) {
        ids.push(report.id);
      }
    }

    if (ids.length === 0) {
      return;
    }

    const previousText = this.sendReportsButton.innerText;

    this.sendReportsButton.disabled = true;
    this.sendReportsButton.innerText = 'Sending...';

    this.handler.sendReports(ids).then(() => {
      this.sendReportsButton.innerText = previousText;
    });
  }
}

const registrationTypeText: Readonly<Record<RegistrationType, string>> = {
  [RegistrationType.kSource]: 'Source',
  [RegistrationType.kTrigger]: 'Trigger',
};

const osRegistrationResultText:
    Readonly<Record<OsRegistrationResult, string>> = {
      [OsRegistrationResult.kPassedToOs]: 'Passed to OS',
      [OsRegistrationResult.kUnsupported]: 'Unsupported',
      [OsRegistrationResult.kInvalidRegistrationUrl]:
          'Invalid registration URL',
      [OsRegistrationResult.kProhibitedByBrowserPolicy]:
          'Prohibited by browser policy',
      [OsRegistrationResult.kExcessiveQueueSize]: 'Excessive queue size',
      [OsRegistrationResult.kRejectedByOs]: 'Rejected by OS',
    };

class OsRegistration {
  timestamp: Date;
  registrationUrl: string;
  topLevelOrigin: string;
  registrationType: string;
  debugKeyAllowed: boolean;
  debugReporting: boolean;
  result: string;

  constructor(mojo: WebUIOsRegistration) {
    this.timestamp = new Date(mojo.time);
    this.registrationUrl = mojo.registrationUrl.url;
    this.topLevelOrigin = originToText(mojo.topLevelOrigin);
    this.debugKeyAllowed = mojo.isDebugKeyAllowed;
    this.debugReporting = mojo.debugReporting;

    this.registrationType = `OS ${registrationTypeText[mojo.type]}`;
    this.result = osRegistrationResultText[mojo.result];
  }
}

class OsRegistrationTableModel extends ArrayTableModel<OsRegistration> {
  constructor() {
    super(
        [
          ValueColumn.of('Timestamp', 'timestamp', asDate),
          ValueColumn.of(
              'Registration Type', 'registrationType', asStringOrBool),
          ValueColumn.of('Registration URL', 'registrationUrl', asUrl),
          ValueColumn.of('Top-Level Origin', 'topLevelOrigin', asUrl),
          ValueColumn.of(
              'Debug Key Allowed', 'debugKeyAllowed', asStringOrBool),
          ValueColumn.of('Debug Reporting', 'debugReporting', asStringOrBool),
          ValueColumn.of('Result', 'result', asStringOrBool),
        ],
        0,
        'No OS registrations.',
    );
  }
}

class DebugReport {
  body: string;
  url: string;
  time: Date;
  status: string;

  constructor(mojo: WebUIDebugReport) {
    this.body = mojo.body;
    this.url = mojo.url.url;
    this.time = new Date(mojo.time);

    if (mojo.status.httpResponseCode !== undefined) {
      this.status = `HTTP ${mojo.status.httpResponseCode}`;
    } else if (mojo.status.networkError !== undefined) {
      this.status = `Network error: ${mojo.status.networkError}`;
    } else {
      throw new Error('invalid DebugReportStatus union');
    }
  }
}

class DebugReportTableModel extends ArrayTableModel<DebugReport> {
  constructor() {
    super(
        [
          ValueColumn.of('Time', 'time', asDate),
          ValueColumn.of('URL', 'url', asUrl),
          ValueColumn.of('Status', 'status', asStringOrBool),
          ValueColumn.of('Body', 'body', asCode),
        ],
        0,  // Sort by report time by default.
        'No verbose debug reports.',
    );
  }

  // TODO(apaseltiner): Style error rows like `ReportTableModel`
}

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

const sourceTypeText: Readonly<Record<SourceType, string>> = {
  [SourceType.kNavigation]: 'Navigation',
  [SourceType.kEvent]: 'Event',
};

const triggerDataMatchingText: Readonly<Record<TriggerDataMatching, string>> = {
  [TriggerDataMatching.kModulus]: 'modulus',
  [TriggerDataMatching.kExact]: 'exact',
};

const attributabilityText:
    Readonly<Record<WebUISource_Attributability, string>> = {
      [WebUISource_Attributability.kAttributable]: 'Attributable',
      [WebUISource_Attributability.kNoisedNever]:
          'Unattributable: noised with no reports',
      [WebUISource_Attributability.kNoisedFalsely]:
          'Unattributable: noised with fake reports',
      [WebUISource_Attributability.kReachedEventLevelAttributionLimit]:
          'Attributable: reached event-level attribution limit',
    };

const sourceRegistrationStatusText:
    Readonly<Record<StoreSourceResult, string>> = {
      [StoreSourceResult.kSuccess]: 'Success',
      [StoreSourceResult.kSuccessNoised]: 'Success',
      [StoreSourceResult.kInternalError]: 'Rejected: internal error',
      [StoreSourceResult.kInsufficientSourceCapacity]:
          'Rejected: insufficient source capacity',
      [StoreSourceResult.kInsufficientUniqueDestinationCapacity]:
          'Rejected: insufficient unique destination capacity',
      [StoreSourceResult.kExcessiveReportingOrigins]:
          'Rejected: excessive reporting origins',
      [StoreSourceResult.kProhibitedByBrowserPolicy]:
          'Rejected: prohibited by browser policy',
      [StoreSourceResult.kDestinationReportingLimitReached]:
          'Rejected: destination reporting limit reached',
      [StoreSourceResult.kDestinationGlobalLimitReached]:
          'Rejected: destination global limit reached',
      [StoreSourceResult.kDestinationBothLimitsReached]:
          'Rejected: destination both limits reached',
      [StoreSourceResult.kExceedsMaxChannelCapacity]:
          'Rejected: channel capacity exceeds max allowed',
      [StoreSourceResult.kReportingOriginsPerSiteLimitReached]:
          'Rejected: reached reporting origins per site limit',
    };

const commonResult = {
  success: 'Success: Report stored',
  internalError: 'Failure: Internal error',
  noMatchingImpressions: 'Failure: No matching sources',
  noMatchingSourceFilterData: 'Failure: No matching source filter data',
  deduplicated: 'Failure: Deduplicated against an earlier report',
  noCapacityForConversionDestination:
      'Failure: No report capacity for destination site',
  excessiveAttributions: 'Failure: Excessive attributions',
  excessiveReportingOrigins: 'Failure: Excessive reporting origins',
  reportWindowPassed: 'Failure: Report window has passed',
  excessiveReports: 'Failure: Excessive reports',
  prohibitedByBrowserPolicy: 'Failure: Prohibited by browser policy',
};

const eventLevelResultText: Readonly<Record<EventLevelResult, string>> = {
  [EventLevelResult.kSuccess]: commonResult.success,
  [EventLevelResult.kSuccessDroppedLowerPriority]: commonResult.success,
  [EventLevelResult.kInternalError]: commonResult.internalError,
  [EventLevelResult.kNoMatchingImpressions]: commonResult.noMatchingImpressions,
  [EventLevelResult.kNoMatchingSourceFilterData]:
      commonResult.noMatchingSourceFilterData,
  [EventLevelResult.kNoCapacityForConversionDestination]:
      commonResult.noCapacityForConversionDestination,
  [EventLevelResult.kExcessiveAttributions]: commonResult.excessiveAttributions,
  [EventLevelResult.kExcessiveReportingOrigins]:
      commonResult.excessiveReportingOrigins,
  [EventLevelResult.kDeduplicated]: commonResult.deduplicated,
  [EventLevelResult.kReportWindowNotStarted]:
      'Failure: Report window has not started',
  [EventLevelResult.kReportWindowPassed]: commonResult.reportWindowPassed,
  [EventLevelResult.kPriorityTooLow]: 'Failure: Priority too low',
  [EventLevelResult.kNeverAttributedSource]: 'Failure: Noised',
  [EventLevelResult.kFalselyAttributedSource]: 'Failure: Noised',
  [EventLevelResult.kNotRegistered]: 'Failure: No event-level data present',
  [EventLevelResult.kProhibitedByBrowserPolicy]:
      commonResult.prohibitedByBrowserPolicy,
  [EventLevelResult.kNoMatchingConfigurations]:
      'Failure: no matching event-level configurations',
  [EventLevelResult.kExcessiveReports]: commonResult.excessiveReports,
  [EventLevelResult.kNoMatchingTriggerData]:
      'Failure: no matching trigger data',
};

const aggregatableResultText: Readonly<Record<AggregatableResult, string>> = {
  [AggregatableResult.kSuccess]: commonResult.success,
  [AggregatableResult.kInternalError]: commonResult.internalError,
  [AggregatableResult.kNoMatchingImpressions]:
      commonResult.noMatchingImpressions,
  [AggregatableResult.kNoMatchingSourceFilterData]:
      commonResult.noMatchingSourceFilterData,
  [AggregatableResult.kNoCapacityForConversionDestination]:
      commonResult.noCapacityForConversionDestination,
  [AggregatableResult.kExcessiveAttributions]:
      commonResult.excessiveAttributions,
  [AggregatableResult.kExcessiveReportingOrigins]:
      commonResult.excessiveReportingOrigins,
  [AggregatableResult.kDeduplicated]: commonResult.deduplicated,
  [AggregatableResult.kReportWindowPassed]: commonResult.reportWindowPassed,
  [AggregatableResult.kNoHistograms]: 'Failure: No source histograms',
  [AggregatableResult.kInsufficientBudget]: 'Failure: Insufficient budget',
  [AggregatableResult.kNotRegistered]: 'Failure: No aggregatable data present',
  [AggregatableResult.kProhibitedByBrowserPolicy]:
      commonResult.prohibitedByBrowserPolicy,
  [AggregatableResult.kExcessiveReports]: commonResult.excessiveReports,
};

const attributionSupportText: Readonly<Record<AttributionSupport, string>> = {
  [AttributionSupport.kWeb]: 'web',
  [AttributionSupport.kWebAndOs]: 'os, web',
  [AttributionSupport.kOs]: 'os',
  [AttributionSupport.kNone]: '',
};

class AttributionInternals implements ObserverInterface {
  private readonly sources = new SourceTableModel();
  private readonly sourceRegistrations = new SourceRegistrationTableModel();
  private readonly triggers = new TriggerTableModel();
  private readonly debugReports = new DebugReportTableModel();
  private readonly osRegistrations = new OsRegistrationTableModel();
  private readonly eventLevelReports: ReportTableModel<EventLevelReport>;
  private readonly aggregatableReports:
      ReportTableModel<AggregatableAttributionReport>;

  private readonly handler = new HandlerRemote();

  constructor() {
    this.eventLevelReports = new ReportTableModel(
        document.querySelector('#event-level-report-controls')!, this.handler, [
          ValueColumn.of('Report Priority', 'reportPriority', asNumber),
          ValueColumn.of(
              'Randomized Report', 'randomizedReport', asStringOrBool),
        ]);

    this.aggregatableReports = new ReportTableModel(
        document.querySelector('#aggregatable-report-controls')!, this.handler,
        [
          ValueColumn.of('Histograms', 'contributions', asCode),
          ValueColumn.of(
              'Verification Token', 'verificationToken', asStringOrBool),
          ValueColumn.of(
              'Aggregation Coordinator', 'aggregationCoordinator', asUrl),
          ValueColumn.of('Null Report', 'isNullReport', asStringOrBool),
        ]);

    installUnreadIndicator(
        this.sources, document.querySelector<HTMLElement>('#sources-tab')!);

    installUnreadIndicator(
        this.sourceRegistrations,
        document.querySelector<HTMLElement>('#source-registrations-tab')!);

    installUnreadIndicator(
        this.triggers, document.querySelector<HTMLElement>('#triggers-tab')!);

    installUnreadIndicator(
        this.eventLevelReports,
        document.querySelector<HTMLElement>('#event-level-reports-tab')!);

    installUnreadIndicator(
        this.aggregatableReports,
        document.querySelector<HTMLElement>('#aggregatable-reports-tab')!);

    installUnreadIndicator(
        this.debugReports,
        document.querySelector<HTMLElement>('#debug-reports-tab')!);

    installUnreadIndicator(
        this.osRegistrations, document.querySelector<HTMLElement>('#os-tab')!);

    installUnreadIndicator(
      this.sources, document.querySelector<HTMLElement>('#filters-tab')!);

    document
        .querySelector<AttributionInternalsTableElement<Source>>(
            '#sourceTable')!.setModel(this.sources);

    document
        .querySelector<AttributionInternalsTableElement<SourceRegistration>>(
            '#sourceRegistrationTable')!.setModel(this.sourceRegistrations);

    document
        .querySelector<AttributionInternalsTableElement<Trigger>>(
            '#triggerTable')!.setModel(this.triggers);

    document
        .querySelector<AttributionInternalsTableElement<EventLevelReport>>(
            '#reportTable')!.setModel(this.eventLevelReports);

    document
        .querySelector<
            AttributionInternalsTableElement<AggregatableAttributionReport>>(
            '#aggregatableReportTable')!.setModel(this.aggregatableReports);

    document
        .querySelector<AttributionInternalsTableElement<DebugReport>>(
            '#debugReportTable')!.setModel(this.debugReports);

    document
        .querySelector<AttributionInternalsTableElement<OsRegistration>>(
            '#osRegistrationTable')!.setModel(this.osRegistrations);
    
    document
        .querySelector<AttributionInternalsTableElement<Source>>(
            '#filterTable')!.setModel(this.sources);

    Factory.getRemote().create(
        new ObserverReceiver(this).$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  onSourcesChanged(): void {
    this.updateSources();
  }

  onReportsChanged(): void {
    this.updateReports();
  }

  onReportSent(mojo: WebUIReport): void {
    this.addSentOrDroppedReport(mojo);
  }

  onDebugReportSent(mojo: WebUIDebugReport): void {
    this.debugReports.addRow(new DebugReport(mojo));
  }

  onReportDropped(mojo: WebUIReport): void {
    this.addSentOrDroppedReport(mojo);
  }

  onSourceHandled(mojo: WebUISourceRegistration): void {
    this.sourceRegistrations.addRow(new SourceRegistration(mojo));
  }

  onTriggerHandled(mojo: WebUITrigger): void {
    this.triggers.addRow(new Trigger(mojo));
  }

  onOsRegistration(mojo: WebUIOsRegistration): void {
    this.osRegistrations.addRow(new OsRegistration(mojo));
  }

  private addSentOrDroppedReport(mojo: WebUIReport): void {
    if (mojo.data.eventLevelData !== undefined) {
      this.eventLevelReports.addSentOrDroppedReport(new EventLevelReport(mojo));
    } else {
      this.aggregatableReports.addSentOrDroppedReport(
          new AggregatableAttributionReport(mojo));
    }
  }

  /**
   * Deletes all data stored by the conversions backend.
   * onReportsChanged and onSourcesChanged will be called
   * automatically as data is deleted, so there's no need to manually refresh
   * the data on completion.
   */
  clearStorage(): void {
    this.sources.clear();
    this.sourceRegistrations.clear();
    this.triggers.clear();
    this.eventLevelReports.clear();
    this.aggregatableReports.clear();
    this.debugReports.clear();
    this.osRegistrations.clear();
    this.handler.clearStorage();
  }

  refresh(): void {
    this.handler.isAttributionReportingEnabled().then((response) => {
      const featureStatus =
          document.querySelector<HTMLElement>('#feature-status')!;
      featureStatus.innerText = response.enabled ? 'enabled' : 'disabled';
      featureStatus.classList.toggle('disabled', !response.enabled);

      const reportDelaysContent =
          document.querySelector<HTMLElement>('#report-delays')!;
      const noiseContent = document.querySelector<HTMLElement>('#noise')!;

      if (response.debugMode) {
        reportDelaysContent.innerText = 'disabled';
        noiseContent.innerText = 'disabled';
      } else {
        reportDelaysContent.innerText = 'enabled';
        noiseContent.innerText = 'enabled';
      }

      const attributionSupport = document.querySelector<HTMLElement>('#attribution-support')!;
      attributionSupport.innerText =
          attributionSupportText[response.attributionSupport];
    });

    this.updateSources();
    this.updateReports();
  }

  private updateSources(): void {
    this.handler.getActiveSources().then(({sources}) => {
      this.sources.setRows(sources.map((mojo) => new Source(mojo)));
    });
  }

  private updateReports(): void {
    this.handler.getReports().then(({reports}) => {
      const eventLevelReports: EventLevelReport[] = [];
      const aggregatableReports: AggregatableAttributionReport[] = [];

      for (const report of reports) {
        if (report.data.eventLevelData !== undefined) {
          eventLevelReports.push(new EventLevelReport(report));
        } else if (report.data.aggregatableAttributionData !== undefined) {
          aggregatableReports.push(new AggregatableAttributionReport(report));
        }
      }

      this.eventLevelReports.setStoredReports(eventLevelReports);
      this.aggregatableReports.setStoredReports(aggregatableReports);
    });
  }
}

function installUnreadIndicator<T>(
    model: TableModel<T>, tab: HTMLElement): void {
  model.rowsChangedListeners.add(() => {
    if (!tab.hasAttribute('selected') && !model.empty()) {
      tab.classList.add('unread');
    }
  });
}

document.addEventListener('DOMContentLoaded', function() {
  const tabBox = document.querySelector('cr-tab-box')!;
  tabBox.addEventListener('selected-index-change', e => {
    const tabs = document.querySelectorAll<HTMLElement>('div[slot="tab"]');
    tabs[(e as CustomEvent<number>).detail]!.classList.remove('unread');
  });

  const internals = new AttributionInternals();

  document.querySelector('#refresh')!.addEventListener(
      'click', () => internals.refresh());
  document.querySelector('#clear-data')!.addEventListener(
      'click', () => internals.clearStorage());

  tabBox.hidden = false;

  internals.refresh();
});

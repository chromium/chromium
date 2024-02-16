// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './attribution_internals_table.js';

import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {AggregatableResult} from './aggregatable_result.mojom-webui.js';
import type {TriggerVerification} from './attribution.mojom-webui.js';
import {AttributionSupport} from './attribution.mojom-webui.js';
import type {HandlerInterface, ObserverInterface, ReportID, ReportStatus, WebUIDebugReport, WebUIOsRegistration, WebUIRegistration, WebUIReport, WebUISource, WebUISourceRegistration, WebUITrigger} from './attribution_internals.mojom-webui.js';
import {Factory, HandlerRemote, ObserverReceiver, WebUISource_Attributability} from './attribution_internals.mojom-webui.js';
import type {AttributionInternalsTableElement, Column, RenderFunc} from './attribution_internals_table.js';
import {OsRegistrationResult, RegistrationType} from './attribution_reporting.mojom-webui.js';
import {EventLevelResult} from './event_level_result.mojom-webui.js';
import {SourceType} from './source_type.mojom-webui.js';
import {StoreSourceResult} from './store_source_result.mojom-webui.js';
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

function valueColumn<T, K extends keyof T>(
    header: string, key: K, {render, compare}: Valuable<T[K]>): Column<T> {
  return {
    renderHeader: th => th.innerText = header,
    render: (td, row) => render(td, row[key]),
    compare: compare ? (a, b) => compare(a[key], b[key]) : undefined,
  };
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

function renderUrl(td: HTMLElement, url: string): void {
  const a = td.ownerDocument.createElement('a');
  a.target = '_blank';
  a.href = url;
  a.innerText = url;
  td.append(a);
}

const asUrl: Valuable<string> = {
  compare: compareDefault,
  render: renderUrl,
};

function isAttributionSuccessDebugReport(url: string): boolean {
  return url.includes('/.well-known/attribution-reporting/debug/');
}

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
  readonly selectionChangedListeners: Set<(anySelected: boolean) => void> =
      new Set();

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

  private onChange(): void {
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

  private notifySelectionChanged(anySelected: boolean): void {
    this.selectionChangedListeners.forEach((f) => f(anySelected));
  }
}

interface Source {
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
}

function newSource(mojo: WebUISource): Source {
  return {
    sourceEventId: mojo.sourceEventId,
    sourceOrigin: originToText(mojo.sourceOrigin),
    destinations:
        mojo.destinations.destinations.map(d => originToText(d.siteAsOrigin))
            .sort(compareDefault),
    reportingOrigin: originToText(mojo.reportingOrigin),
    sourceTime: new Date(mojo.sourceTime),
    expiryTime: new Date(mojo.expiryTime),
    triggerSpecs: mojo.triggerSpecsJson,
    aggregatableReportWindowTime: new Date(mojo.aggregatableReportWindowTime),
    maxEventLevelReports: mojo.maxEventLevelReports,
    sourceType: sourceTypeText[mojo.sourceType],
    priority: mojo.priority,
    filterData: JSON.stringify(mojo.filterData.filterValues, null, ' '),
    aggregationKeys: JSON.stringify(mojo.aggregationKeys, bigintReplacer, ' '),
    // TODO(crbug.com/1442785): Workaround for undefined/null issue.
    debugKey: typeof mojo.debugKey === 'bigint' ? mojo.debugKey : undefined,
    dedupKeys: mojo.dedupKeys.sort(compareDefault),
    aggregatableBudgetConsumed: mojo.aggregatableBudgetConsumed,
    aggregatableDedupKeys: mojo.aggregatableDedupKeys.sort(compareDefault),
    triggerDataMatching: triggerDataMatchingText[mojo.triggerDataMatching],
    eventLevelEpsilon: mojo.eventLevelEpsilon,
    status: attributabilityText[mojo.attributability],
    debugCookieSet: mojo.debugCookieSet,
  };
}

function initSourceTable(
    t: AttributionInternalsTableElement<Source>,
    model: TableModel<Source>): void {
  t.init(model, [
    valueColumn('Source Event ID', 'sourceEventId', asNumber),
    valueColumn('Status', 'status', asStringOrBool),
    valueColumn('Source Origin', 'sourceOrigin', asUrl),
    valueColumn('Destinations', 'destinations', asList(asUrl)),
    valueColumn('Reporting Origin', 'reportingOrigin', asUrl),
    {
      ...valueColumn('Registration Time', 'sourceTime', asDate),
      defaultSort: true,
    },
    valueColumn('Expiry Time', 'expiryTime', asDate),
    valueColumn('Trigger Specs', 'triggerSpecs', asCode),
    valueColumn(
        'Aggregatable Report Window Time', 'aggregatableReportWindowTime',
        asDate),
    valueColumn('Max Event Level Reports', 'maxEventLevelReports', asNumber),
    valueColumn('Source Type', 'sourceType', asStringOrBool),
    valueColumn('Priority', 'priority', asNumber),
    valueColumn('Filter Data', 'filterData', asCode),
    valueColumn('Aggregation Keys', 'aggregationKeys', asCode),
    valueColumn('Trigger Data Matching', 'triggerDataMatching', asStringOrBool),
    valueColumn(
        'Event-Level Epsilon', 'eventLevelEpsilon',
        asCustomNumber((v: number) => v.toFixed(3))),
    valueColumn(
        'Aggregatable Budget Consumed', 'aggregatableBudgetConsumed',
        asCustomNumber((v) => `${v} / ${BUDGET_PER_SOURCE}`)),
    valueColumn('Debug Key', 'debugKey', allowingUndefined(asNumber)),
    valueColumn('Debug Cookie Set', 'debugCookieSet', asStringOrBool),
    valueColumn('Dedup Keys', 'dedupKeys', asList(asNumber)),
    valueColumn(
        'Aggregatable Dedup Keys', 'aggregatableDedupKeys', asList(asNumber)),
  ]);
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

function initRegistrationTableModel<T extends Registration>(
    t: AttributionInternalsTableElement<T>, model: TableModel<T>,
    contextOriginTitle: string, cols: Iterable<Column<T>>): void {
  t.init(model, [
    {...valueColumn('Time', 'time', asDate), defaultSort: true},
    valueColumn(contextOriginTitle, 'contextOrigin', asUrl),
    valueColumn('Reporting Origin', 'reportingOrigin', asUrl),
    valueColumn('Registration JSON', 'registrationJson', asCode),
    valueColumn(
        'Cleared Debug Key', 'clearedDebugKey', allowingUndefined(asNumber)),
    ...cols,
  ]);
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
  valueColumn('Token', 'token', asStringOrBool),
  valueColumn('Report ID', 'aggregatableReportId', asStringOrBool),
];

const reportVerificationColumn: Column<Trigger> = {
  renderHeader(th: HTMLElement): void {
    th.innerText = 'Report Verification';
  },

  render(td: HTMLElement, row: Trigger): void {
    for (const verification of row.verifications) {
      renderDL(td, verification, VERIFICATION_COLS);
    }
  },
};

function initTriggerTable(
    t: AttributionInternalsTableElement<Trigger>,
    model: TableModel<Trigger>): void {
  initRegistrationTableModel(t, model, 'Destination', [
    valueColumn('Event-Level Result', 'eventLevelResult', asStringOrBool),
    valueColumn('Aggregatable Result', 'aggregatableResult', asStringOrBool),
    reportVerificationColumn,
  ]);
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

function initSourceRegistrationTable(
    t: AttributionInternalsTableElement<SourceRegistration>,
    model: TableModel<SourceRegistration>): void {
  initRegistrationTableModel(t, model, 'Source Origin', [
    valueColumn('Type', 'type', asStringOrBool),
    valueColumn('Status', 'status', asStringOrBool),
  ]);
}

function isHttpError(code: number): boolean {
  return code < 200 || code >= 400;
}

function styleReportRow(tr: HTMLElement, report: {sendFailed: boolean}): void {
  tr.classList.toggle('send-error', report.sendFailed);
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

    [this.status, this.sendFailed] =
        Report.statusToString(mojo.status, 'Sent: ');
  }

  static statusToString(status: ReportStatus, sentPrefix: string):
      [status: string, sendFailed: boolean] {
    if (status.sent !== undefined) {
      return [
        `${sentPrefix}HTTP ${status.sent}`,
        isHttpError(status.sent),
      ];
    } else if (status.pending !== undefined) {
      return ['Pending', false];
    } else if (status.replacedByHigherPriorityReport !== undefined) {
      return [
        `Replaced by higher-priority report: ${
            status.replacedByHigherPriorityReport}`,
        false,
      ];
    } else if (status.prohibitedByBrowserPolicy !== undefined) {
      return ['Prohibited by browser policy', false];
    } else if (status.networkError !== undefined) {
      return [`Network error: ${status.networkError}`, true];
    } else if (status.failedToAssemble !== undefined) {
      return ['Dropped due to assembly failure', false];
    } else {
      throw new Error('invalid ReportStatus union');
    }
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

class AggregatableReport extends Report {
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

function initReportTable<T extends Report>(
    t: AttributionInternalsTableElement<T>, model: ReportTableModel<T>,
    container: HTMLElement, handler: HandlerInterface,
    cols: Iterable<Column<T>>): void {
  const selectionColumn = new SelectionColumn(model);

  t.init(
      model,
      [
        selectionColumn,
        valueColumn('Status', 'status', asStringOrBool),
        valueColumn('Report URL', 'reportUrl', asUrl),
        valueColumn('Trigger Time', 'triggerTime', asDate),
        {
          ...valueColumn('Report Time', 'reportTime', asDate),
          defaultSort: true,
        },
        ...cols,
        valueColumn('Report Body', 'reportBody', asCode),
      ],
      styleReportRow,
  );

  const sendReportsButton = container.querySelector('button')!;

  sendReportsButton.addEventListener(
      'click', () => model.sendReports(sendReportsButton, handler));
  selectionColumn.selectionChangedListeners.add((anySelected: boolean) => {
    sendReportsButton.disabled = !anySelected;
  });
}

class ReportTableModel<T extends Report> extends TableModel<T> {
  private sentOrDroppedReports: T[] = [];
  private storedReports: T[] = [];

  constructor() {
    super();
  }

  override rowCount(): number {
    return this.sentOrDroppedReports.length + this.storedReports.length;
  }

  override getRows(): T[] {
    return this.sentOrDroppedReports.concat(this.storedReports);
  }

  setStoredReports(storedReports: T[]): void {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  addSentOrDroppedReport(report: T): void {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.sentOrDroppedReports.length >= 1000) {
      this.sentOrDroppedReports = [];
    }

    this.sentOrDroppedReports.push(report);
    this.notifyRowsChanged();
  }

  clear(): void {
    this.storedReports = [];
    this.sentOrDroppedReports = [];
    this.notifyRowsChanged();
  }

  /**
   * Sends all selected reports.
   * Disables the button while the reports are still being sent.
   * Observer.onReportsChanged and Observer.onSourcesChanged will be called
   * automatically as reports are deleted, so there's no need to manually
   * refresh the data on completion.
   */
  sendReports(sendReportsButton: HTMLButtonElement, handler: HandlerInterface):
      void {
    const ids: ReportID[] = [];
    for (const report of this.storedReports) {
      if (!report.input.disabled && report.input.checked) {
        ids.push(report.id);
      }
    }

    if (ids.length === 0) {
      return;
    }

    const previousText = sendReportsButton.innerText;

    sendReportsButton.disabled = true;
    sendReportsButton.innerText = 'Sending...';

    handler.sendReports(ids).then(() => {
      sendReportsButton.innerText = previousText;
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

interface OsRegistration {
  time: Date;
  registrationUrl: string;
  topLevelOrigin: string;
  registrationType: string;
  debugKeyAllowed: boolean;
  debugReporting: boolean;
  result: string;
}

function newOsRegistration(mojo: WebUIOsRegistration): OsRegistration {
  return {
    time: new Date(mojo.time),
    registrationUrl: mojo.registrationUrl.url,
    topLevelOrigin: originToText(mojo.topLevelOrigin),
    debugKeyAllowed: mojo.isDebugKeyAllowed,
    debugReporting: mojo.debugReporting,
    registrationType: `OS ${registrationTypeText[mojo.type]}`,
    result: osRegistrationResultText[mojo.result],
  };
}

function initOsRegistrationTable(
    t: AttributionInternalsTableElement<OsRegistration>,
    model: TableModel<OsRegistration>): void {
  t.init(model, [
    {...valueColumn('Time', 'time', asDate), defaultSort: true},
    valueColumn('Registration Type', 'registrationType', asStringOrBool),
    valueColumn('Registration URL', 'registrationUrl', asUrl),
    valueColumn('Top-Level Origin', 'topLevelOrigin', asUrl),
    valueColumn('Debug Key Allowed', 'debugKeyAllowed', asStringOrBool),
    valueColumn('Debug Reporting', 'debugReporting', asStringOrBool),
    valueColumn('Result', 'result', asStringOrBool),
  ]);
}

interface DebugReport {
  body: string;
  url: string;
  time: Date;
  status: string;
  sendFailed: boolean;
}

function verboseDebugReport(mojo: WebUIDebugReport): DebugReport {
  const report: DebugReport = {
    body: mojo.body,
    url: mojo.url.url,
    time: new Date(mojo.time),
    status: '',
    sendFailed: false,
  };

  if (mojo.status.httpResponseCode !== undefined) {
    report.status = `HTTP ${mojo.status.httpResponseCode}`;
    report.sendFailed = isHttpError(mojo.status.httpResponseCode);
  } else if (mojo.status.networkError !== undefined) {
    report.status = `Network error: ${mojo.status.networkError}`;
    report.sendFailed = true;
  } else {
    throw new Error('invalid DebugReportStatus union');
  }

  return report;
}

function attributionSuccessDebugReport(mojo: WebUIReport): DebugReport {
  const [status, sendFailed] =
      Report.statusToString(mojo.status, /*sentPrefix=*/ '');
  return {
    body: mojo.reportBody,
    url: mojo.reportUrl.url,
    time: new Date(mojo.reportTime),
    status,
    sendFailed,
  };
}

function initDebugReportTable(
    t: AttributionInternalsTableElement<DebugReport>,
    model: TableModel<DebugReport>): void {
  t.init(
      model,
      [
        {...valueColumn('Time', 'time', asDate), defaultSort: true},
        valueColumn('URL', 'url', asUrl),
        valueColumn('Status', 'status', asStringOrBool),
        valueColumn('Body', 'body', asCode),
      ],
      styleReportRow,
  );
}

// Converts a mojo origin into a user-readable string, omitting default ports.
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
  private readonly sources = new ArrayTableModel<Source>();
  private readonly sourceRegistrations =
      new ArrayTableModel<SourceRegistration>();
  private readonly triggers = new ArrayTableModel<Trigger>();
  private readonly debugReports = new ArrayTableModel<DebugReport>();
  private readonly osRegistrations = new ArrayTableModel<OsRegistration>();
  private readonly eventLevelReports = new ReportTableModel<EventLevelReport>();
  private readonly aggregatableReports =
      new ReportTableModel<AggregatableReport>();

  private readonly handler = new HandlerRemote();

  constructor() {
    initReportTable<EventLevelReport>(
        document.querySelector('#reportTable')!, this.eventLevelReports,
        document.querySelector('#event-level-report-controls')!, this.handler, [
          valueColumn('Report Priority', 'reportPriority', asNumber),
          valueColumn('Randomized Report', 'randomizedReport', asStringOrBool),
        ]);

    initReportTable<AggregatableReport>(
        document.querySelector('#aggregatableReportTable')!,
        this.aggregatableReports,
        document.querySelector('#aggregatable-report-controls')!, this.handler,
        [
          valueColumn('Histograms', 'contributions', asCode),
          valueColumn(
              'Verification Token', 'verificationToken', asStringOrBool),
          valueColumn(
              'Aggregation Coordinator', 'aggregationCoordinator', asUrl),
          valueColumn('Null Report', 'isNullReport', asStringOrBool),
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

    initSourceTable(document.querySelector('#sourceTable')!, this.sources);

    initSourceRegistrationTable(
        document.querySelector('#sourceRegistrationTable')!,
        this.sourceRegistrations);

    initTriggerTable(document.querySelector('#triggerTable')!, this.triggers);

    initDebugReportTable(
        document.querySelector('#debugReportTable')!, this.debugReports);

    initOsRegistrationTable(
        document.querySelector('#osRegistrationTable')!, this.osRegistrations);

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
    this.debugReports.addRow(verboseDebugReport(mojo));
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
    this.osRegistrations.addRow(newOsRegistration(mojo));
  }

  private addSentOrDroppedReport(mojo: WebUIReport): void {
    if (isAttributionSuccessDebugReport(mojo.reportUrl.url)) {
      this.debugReports.addRow(attributionSuccessDebugReport(mojo));
    } else if (mojo.data.eventLevelData !== undefined) {
      this.eventLevelReports.addSentOrDroppedReport(new EventLevelReport(mojo));
    } else {
      this.aggregatableReports.addSentOrDroppedReport(
          new AggregatableReport(mojo));
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
      this.sources.setRows(sources.map(newSource));
    });
  }

  private updateReports(): void {
    this.handler.getReports().then(({reports}) => {
      const eventLevelReports: EventLevelReport[] = [];
      const aggregatableReports: AggregatableReport[] = [];

      for (const report of reports) {
        if (report.data.eventLevelData !== undefined) {
          eventLevelReports.push(new EventLevelReport(report));
        } else if (report.data.aggregatableAttributionData !== undefined) {
          aggregatableReports.push(new AggregatableReport(report));
        }
      }

      this.eventLevelReports.setStoredReports(eventLevelReports);
      this.aggregatableReports.setStoredReports(aggregatableReports);
    });
  }
}

function installUnreadIndicator<T>(
    model: TableModel<T>, tab: HTMLElement): void {
  model.rowsChangedListeners.add(
      () => tab.classList.toggle(
          'unread', !tab.hasAttribute('selected') && model.rowCount() > 0));
}

document.addEventListener('DOMContentLoaded', function() {
  const tabBox = document.querySelector('cr-tab-box')!;
  tabBox.addEventListener('selected-index-change', e => {
    const tabs = document.querySelectorAll<HTMLElement>('div[slot="tab"]');
    tabs[e.detail]!.classList.remove('unread');
  });

  const internals = new AttributionInternals();

  document.querySelector('#refresh')!.addEventListener(
      'click', () => internals.refresh());
  document.querySelector('#clear-data')!.addEventListener(
      'click', () => internals.clearStorage());

  tabBox.hidden = false;

  internals.refresh();
});

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './attribution_detail_table.js';
import './attribution_internals_table.js';

import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {AggregatableResult} from './aggregatable_result.mojom-webui.js';
import {AttributionSupport} from './attribution.mojom-webui.js';
import type {AttributionDetailTableElement} from './attribution_detail_table.js';
import type {HandlerInterface, NetworkStatus, ObserverInterface, ReportID, ReportStatus, WebUIAggregatableDebugReport, WebUIDebugReport, WebUIOsRegistration, WebUIRegistration, WebUIReport, WebUISource, WebUISourceRegistration, WebUITrigger} from './attribution_internals.mojom-webui.js';
import {Factory, HandlerRemote, ObserverReceiver, WebUISource_Attributability} from './attribution_internals.mojom-webui.js';
import type {AttributionInternalsTableElement, CompareFunc, DataColumn, InitOpts, RenderFunc} from './attribution_internals_table.js';
import {OsRegistrationResult, RegistrationType} from './attribution_reporting.mojom-webui.js';
import {EventLevelResult} from './event_level_result.mojom-webui.js';
import {ProcessAggregatableDebugReportResult} from './process_aggregatable_debug_report_result.mojom-webui.js';
import {SourceType} from './source_type.mojom-webui.js';
import {StoreSourceResult} from './store_source_result.mojom-webui.js';
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
    label: string, key: K, {render, compare}: Valuable<T[K]>,
    defaultSort: boolean = false): DataColumn<T> {
  return {
    label,
    render: (td, data) => render(td, data[key]),
    compare: compare ? (a, b) => compare(a[key], b[key]) : undefined,
    defaultSort,
  };
}

const asDate: Valuable<Date> = {
  compare: compareDefault,
  render: (td: HTMLElement, v: Date) => {
    const time = td.ownerDocument.createElement('time');
    time.dateTime = v.toISOString();
    time.innerText = v.toLocaleString();
    td.replaceChildren(time);
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

    td.replaceChildren(pre);
  },
};

function asList<V>({render, compare}: Valuable<V>): Valuable<V[]> {
  return {
    compare: compare ? compareLexicographic(compare) : undefined,
    render: (td: HTMLElement, vs: V[]) => {
      if (vs.length === 0) {
        td.replaceChildren();
        return;
      }

      const ul = td.ownerDocument.createElement('ul');

      for (const v of vs) {
        const li = td.ownerDocument.createElement('li');
        render(li, v);
        ul.append(li);
      }

      td.replaceChildren(ul);
    },
  };
}

function renderUrl(td: HTMLElement, url: string): void {
  const a = td.ownerDocument.createElement('a');
  a.target = '_blank';
  a.href = url;
  a.innerText = url;
  td.replaceChildren(a);
}

const asUrl: Valuable<string> = {
  compare: compareDefault,
  render: renderUrl,
};

function isAttributionSuccessDebugReport(url: string): boolean {
  return url.includes('/.well-known/attribution-reporting/debug/');
}

interface Source {
  id: bigint;
  sourceEventId: bigint;
  sourceOrigin: string;
  destinations: string[];
  reportingOrigin: string;
  sourceTime: Date;
  expiryTime: Date;
  triggerSpecs: string;
  aggregatableReportWindowTime: Date;
  sourceType: string;
  filterData: string;
  aggregationKeys: string;
  debugKey?: bigint;
  dedupKeys: bigint[];
  priority: bigint;
  status: string;
  remainingAggregatableAttributionBudget: number;
  aggregatableDedupKeys: bigint[];
  triggerDataMatching: string;
  eventLevelEpsilon: number;
  debugCookieSet: boolean;
  remainingAggregatableDebugBudget: number;
  aggregatableDebugKeyPiece: string;
  attributionScopesData: string;
}

function newSource(mojo: WebUISource): Source {
  return {
    id: mojo.id,
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
    sourceType: sourceTypeText[mojo.sourceType],
    priority: mojo.priority,
    filterData: JSON.stringify(mojo.filterData.filterValues, null, ' '),
    aggregationKeys: JSON.stringify(mojo.aggregationKeys, bigintReplacer, ' '),
    debugKey: mojo.debugKey ?? undefined,
    dedupKeys: mojo.dedupKeys.sort(compareDefault),
    remainingAggregatableAttributionBudget:
        mojo.remainingAggregatableAttributionBudget,
    aggregatableDedupKeys: mojo.aggregatableDedupKeys.sort(compareDefault),
    triggerDataMatching: triggerDataMatchingText[mojo.triggerDataMatching],
    eventLevelEpsilon: mojo.eventLevelEpsilon,
    status: attributabilityText[mojo.attributability],
    debugCookieSet: mojo.debugCookieSet,
    remainingAggregatableDebugBudget: mojo.remainingAggregatableDebugBudget,
    aggregatableDebugKeyPiece: mojo.aggregatableDebugKeyPiece,
    attributionScopesData: mojo.attributionScopesDataJson,
  };
}

function initSourceTable(panel: HTMLElement):
    AttributionInternalsTableElement<Source> {
  return initPanel(
      panel,
      [
        valueColumn('Source Event ID', 'sourceEventId', asNumber),
        valueColumn('Status', 'status', asStringOrBool),
        valueColumn('Source Origin', 'sourceOrigin', asUrl),
        valueColumn('Destinations', 'destinations', asList(asUrl)),
        valueColumn('Reporting Origin', 'reportingOrigin', asUrl),
        valueColumn(
            'Registration Time', 'sourceTime', asDate, /*defaultSort=*/ true),
        valueColumn('Expiry', 'expiryTime', asDate),
        valueColumn('Source Type', 'sourceType', asStringOrBool),
        valueColumn('Debug Key', 'debugKey', allowingUndefined(asNumber)),
      ],
      {
        getId: source => source.id,
        isSelectable: true,
      },
      [
        valueColumn('Priority', 'priority', asNumber),
        valueColumn('Filter Data', 'filterData', asCode),
        valueColumn('Debug Cookie Set', 'debugCookieSet', asStringOrBool),
        'Event-Level Fields',
        valueColumn('Attribution Scopes Data', 'attributionScopesData', asCode),
        valueColumn(
            'Epsilon', 'eventLevelEpsilon',
            asCustomNumber((v: number) => v.toFixed(3))),
        valueColumn(
            'Trigger Data Matching', 'triggerDataMatching', asStringOrBool),
        valueColumn('Trigger Specs', 'triggerSpecs', asCode),
        valueColumn('Dedup Keys', 'dedupKeys', asList(asNumber)),
        'Aggregatable Fields',
        valueColumn(
            'Report Window Time', 'aggregatableReportWindowTime', asDate),
        valueColumn(
            'Remaining Aggregatable Attribution Budget',
            'remainingAggregatableAttributionBudget',
            asCustomNumber((v) => `${v} / ${BUDGET_PER_SOURCE}`)),
        valueColumn('Aggregation Keys', 'aggregationKeys', asCode),
        valueColumn('Dedup Keys', 'aggregatableDedupKeys', asList(asNumber)),
        valueColumn(
            'Remaining Aggregatable Debug Budget',
            'remainingAggregatableDebugBudget',
            asCustomNumber((v) => `${v} / ${BUDGET_PER_SOURCE}`)),
        valueColumn(
            'Aggregatable Debug Key Piece', 'aggregatableDebugKeyPiece',
            asStringOrBool),
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
    this.clearedDebugKey = mojo.clearedDebugKey ?? undefined;
  }
}

function initRegistrationTableModel<T extends Registration>(
    panel: HTMLElement, contextOriginTitle: string,
    cols: Iterable<DataColumn<T>>): AttributionInternalsTableElement<T> {
  return initPanel(
      panel,
      [
        valueColumn('Time', 'time', asDate, /*defaultSort=*/ true),
        valueColumn(contextOriginTitle, 'contextOrigin', asUrl),
        valueColumn('Reporting Origin', 'reportingOrigin', asUrl),
        valueColumn(
            'Cleared Debug Key', 'clearedDebugKey',
            allowingUndefined(asNumber)),
        ...cols,
      ],
      {isSelectable: true},
      [valueColumn('Registration JSON', 'registrationJson', asCode)]);
}

class Trigger extends Registration {
  readonly eventLevelResult: string;
  readonly aggregatableResult: string;

  constructor(mojo: WebUITrigger) {
    super(mojo.registration);
    this.eventLevelResult = eventLevelResultText[mojo.eventLevelResult];
    this.aggregatableResult = aggregatableResultText[mojo.aggregatableResult];
  }
}

function initTriggerTable(panel: HTMLElement):
    AttributionInternalsTableElement<Trigger> {
  return initRegistrationTableModel(panel, 'Destination', [
    valueColumn('Event-Level Result', 'eventLevelResult', asStringOrBool),
    valueColumn('Aggregatable Result', 'aggregatableResult', asStringOrBool),
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

function initSourceRegistrationTable(panel: HTMLElement):
    AttributionInternalsTableElement<SourceRegistration> {
  return initRegistrationTableModel(panel, 'Source Origin', [
    valueColumn('Type', 'type', asStringOrBool),
    valueColumn('Status', 'status', asStringOrBool),
  ]);
}

function isHttpError(code: number): boolean {
  return code < 200 || code >= 400;
}

const reportStatusColumn: DataColumn<{status: string, sendFailed: boolean}> = {
  label: 'Status',
  compare: (a, b) => compareDefault(a.status, b.status),
  render: (td, report) => {
    td.classList.toggle('send-error', report.sendFailed);
    td.innerText = report.status;
  },
};

function networkStatusToString(status: NetworkStatus, sentPrefix: string):
    [status: string, sendFailed: boolean] {
  if (status.httpResponseCode !== undefined) {
    return [
      `${sentPrefix}HTTP ${status.httpResponseCode}`,
      isHttpError(status.httpResponseCode),
    ];
  } else if (status.networkError !== undefined) {
    return [`Network error: ${status.networkError}`, true];
  } else {
    throw new Error('invalid NetworkStatus union');
  }
}

class Report {
  id: ReportID;
  reportBody: string;
  reportUrl: string;
  triggerTime: Date;
  reportTime: Date;
  status: string;
  sendFailed: boolean;

  constructor(mojo: WebUIReport) {
    this.id = mojo.id;
    this.reportBody = mojo.reportBody;
    this.reportUrl = mojo.reportUrl.url;
    this.triggerTime = new Date(mojo.triggerTime);
    this.reportTime = new Date(mojo.reportTime);

    [this.status, this.sendFailed] =
        Report.statusToString(mojo.status, 'Sent: ');
  }

  isPending(): boolean {
    return this.status === 'Pending';
  }

  static statusToString(status: ReportStatus, sentPrefix: string):
      [status: string, sendFailed: boolean] {
    if (status.networkStatus !== undefined) {
      return networkStatusToString(status.networkStatus, sentPrefix);
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
  aggregationCoordinator: string;
  isNullReport: boolean;

  constructor(mojo: WebUIReport) {
    super(mojo);

    this.contributions = JSON.stringify(
        mojo.data.aggregatableAttributionData!.contributions, bigintReplacer,
        ' ');

    this.aggregationCoordinator =
        mojo.data.aggregatableAttributionData!.aggregationCoordinator;

    this.isNullReport = mojo.data.aggregatableAttributionData!.isNullReport;
  }
}

function initPanel<T>(
    panel: HTMLElement, cols: Iterable<DataColumn<T>>, initOpts: InitOpts<T>,
    detailCols: Iterable<string|DataColumn<T>>,
    onSelectionChange: (data: T|undefined) => void =
        () => {}): AttributionInternalsTableElement<T> {
  const t = panel.querySelector<AttributionInternalsTableElement<T>>(
      'attribution-internals-table')!;

  t.init(cols, initOpts);

  const d = panel.querySelector<AttributionDetailTableElement<T>>(
      'attribution-detail-table')!;

  d.init([...cols, ...detailCols]);

  t.addEventListener(
      'selection-change', (e: CustomEvent<{data: T | undefined}>) => {
        onSelectionChange(e.detail.data);
        d.update(e.detail.data);
      });

  d.addEventListener('close', () => t.clearSelection());

  return t;
}

function initReportTable<T extends Report>(
    panel: HTMLElement, handler: HandlerInterface,
    cols: Iterable<DataColumn<T>>): AttributionInternalsTableElement<T> {
  const sendReportButton = panel.querySelector('button')!;

  const t = initPanel<T>(
      panel,
      [
        reportStatusColumn,
        valueColumn('URL', 'reportUrl', asUrl),
        valueColumn('Trigger Time', 'triggerTime', asDate),
        valueColumn('Report Time', 'reportTime', asDate, /*defaultSort=*/ true),
        ...cols,
      ],
      {
        // Prevent sent/dropped reports from being removed by returning
        // undefined.
        getId: (report, updated) =>
            (report.isPending() || updated) ? report.id.value : undefined,
        isSelectable: true,
      },
      [valueColumn('Body', 'reportBody', asCode)],
      (report: T|undefined) => sendReportButton.disabled =
          !(report?.isPending()));

  sendReportButton.addEventListener(
      'click', () => sendReport(t, sendReportButton, handler));

  return t;
}

/**
 * Sends the selected report.
 * Disables the button while the report is still being sent.
 * Observer.onReportsChanged and Observer.onSourcesChanged will be called
 * automatically as the report is deleted, so there's no need to manually
 * refresh the data on completion.
 */
function sendReport<T extends Report>(
    t: AttributionInternalsTableElement<T>, sendReportButton: HTMLButtonElement,
    handler: HandlerInterface): void {
  const id = t.selectedData()?.id;
  if (id === undefined) {
    return;
  }

  const previousText = sendReportButton.innerText;

  sendReportButton.disabled = true;
  sendReportButton.innerText = 'Sending...';

  handler.sendReport(id).then(() => {
    sendReportButton.innerText = previousText;
  });
}

const registrationTypeText: Readonly<Record<RegistrationType, string>> = {
  [RegistrationType.kSource]: 'Source',
  [RegistrationType.kTrigger]: 'Trigger',
};

const osRegistrationResultText:
    Readonly<Record<OsRegistrationResult, string>> = {
      [OsRegistrationResult.kPassedToOs]: 'Passed to OS',
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
    t: AttributionInternalsTableElement<OsRegistration>):
    AttributionInternalsTableElement<OsRegistration> {
  t.init([
    valueColumn('Time', 'time', asDate, /*defaultSort=*/ true),
    valueColumn('Type', 'registrationType', asStringOrBool),
    valueColumn('URL', 'registrationUrl', asUrl),
    valueColumn('Top-Level Origin', 'topLevelOrigin', asUrl),
    valueColumn('Debug Key Allowed', 'debugKeyAllowed', asStringOrBool),
    valueColumn('Debug Reporting', 'debugReporting', asStringOrBool),
    valueColumn('Result', 'result', asStringOrBool),
  ]);
  return t;
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

  [report.status, report.sendFailed] =
      networkStatusToString(mojo.status, /*sentPrefix=*/ '');

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

const processAggregatableDebugReportResultText:
    Readonly<Record<ProcessAggregatableDebugReportResult, string>> = {
      [ProcessAggregatableDebugReportResult.kSuccess]: 'Success',
      [ProcessAggregatableDebugReportResult.kNoDebugData]: 'No debug data',
      [ProcessAggregatableDebugReportResult.kInsufficientBudget]:
          'Insufficient budget',
      [ProcessAggregatableDebugReportResult.kExcessiveReports]:
          'Excessive reports',
      [ProcessAggregatableDebugReportResult.kGlobalRateLimitReached]:
          'Global rate-limit reached',
      [ProcessAggregatableDebugReportResult.kReportingSiteRateLimitReached]:
          'Per reporting site rate-limit reached',
      [ProcessAggregatableDebugReportResult.kBothRateLimitsReached]:
          'Both rate-limits reached',
      [ProcessAggregatableDebugReportResult.kInternalError]: 'Internal error',
    };

function aggregatableDebugReport(mojo: WebUIAggregatableDebugReport):
    DebugReport {
  const report: DebugReport = {
    body: mojo.body,
    url: mojo.url.url,
    time: new Date(mojo.time),
    status: '',
    sendFailed: false,
  };

  const processStatus =
      processAggregatableDebugReportResultText[mojo.processResult];
  let sendStatus;

  if (mojo.sendResult.networkStatus !== undefined) {
    [sendStatus, report.sendFailed] = networkStatusToString(
        mojo.sendResult.networkStatus, /*sentPrefix=*/ '');
  } else if (mojo.sendResult.assemblyFailed !== undefined) {
    sendStatus = 'Assembly failure';
  } else {
    throw new Error('invalid AggregatableDebugReportStatus union');
  }

  report.status = `${processStatus}, ${sendStatus}`;

  return report;
}

function initDebugReportTable(panel: HTMLElement):
    AttributionInternalsTableElement<DebugReport> {
  return initPanel(
      panel,
      [
        valueColumn('Time', 'time', asDate, /*defaultSort=*/ true),
        valueColumn('URL', 'url', asUrl),
        reportStatusColumn,
      ],
      {isSelectable: true}, [
        valueColumn('Body', 'body', asCode),
      ]);
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
      [StoreSourceResult.kExceedsMaxTriggerStateCardinality]:
          'Rejected: trigger state cardinality exceeds limit',
      [StoreSourceResult.kDestinationPerDayReportingLimitReached]:
          'Rejected: destination per day reporting limit reached',
      [StoreSourceResult.kExceedsMaxScopesChannelCapacity]:
          'Rejected: scopes channel capacity exceeds max allowed',
      [StoreSourceResult.kExceedsMaxEventStatesLimit]:
          'Rejected: event states exceeds limit',
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
  [AttributionSupport.kUnset]: 'unset',
};

class AttributionInternals implements ObserverInterface {
  private readonly sources: AttributionInternalsTableElement<Source>;
  private readonly sourceRegistrations:
      AttributionInternalsTableElement<SourceRegistration>;
  private readonly triggers: AttributionInternalsTableElement<Trigger>;
  private readonly debugReports: AttributionInternalsTableElement<DebugReport>;
  private readonly osRegistrations:
      AttributionInternalsTableElement<OsRegistration>;
  private readonly eventLevelReports:
      AttributionInternalsTableElement<EventLevelReport>;
  private readonly aggregatableReports:
      AttributionInternalsTableElement<AggregatableReport>;

  private readonly handler = new HandlerRemote();

  constructor() {
    this.eventLevelReports = initReportTable<EventLevelReport>(
        document.querySelector('#event-level-report-panel')!, this.handler, [
          valueColumn('Priority', 'reportPriority', asNumber),
          valueColumn('Randomized', 'randomizedReport', asStringOrBool),
        ]);

    this.aggregatableReports = initReportTable<AggregatableReport>(
        document.querySelector('#aggregatable-report-panel')!, this.handler, [
          valueColumn('Histograms', 'contributions', asCode),
          valueColumn(
              'Aggregation Coordinator', 'aggregationCoordinator', asUrl),
          valueColumn('Null', 'isNullReport', asStringOrBool),
        ]);

    this.sources =
        initSourceTable(document.querySelector('#active-source-panel')!);

    this.sourceRegistrations = initSourceRegistrationTable(
        document.querySelector('#source-registration-panel')!);

    this.triggers = initTriggerTable(
        document.querySelector('#trigger-registration-panel')!);

    this.debugReports =
        initDebugReportTable(document.querySelector('#debug-report-panel')!);

    this.osRegistrations = initOsRegistrationTable(
        document.querySelector('#osRegistrationTable')!);

    const tabs = document.querySelectorAll<HTMLElement>('div[slot="tab"]');
    const panels = document.querySelectorAll<HTMLElement>('div[slot="panel"]');

    for (let i = 0; i < panels.length && i < tabs.length; ++i) {
      const tab = tabs[i]!;
      panels[i]!.addEventListener(
          'rows-change',
          e => tab.classList.toggle(
              'unread',
              !tab.hasAttribute('selected') && e.detail.rowCount > 0));
    }

    Factory.getRemote().create(
        new ObserverReceiver(this).$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  onReportHandled(mojo: WebUIReport): void {
    this.addSentOrDroppedReport(mojo);
  }

  onDebugReportSent(mojo: WebUIDebugReport): void {
    this.debugReports.addRow(verboseDebugReport(mojo));
  }

  onAggregatableDebugReportSent(mojo: WebUIAggregatableDebugReport): void {
    this.debugReports.addRow(aggregatableDebugReport(mojo));
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
      this.eventLevelReports.addRow(new EventLevelReport(mojo));
    } else {
      this.aggregatableReports.addRow(new AggregatableReport(mojo));
    }
  }

  /**
   * Deletes all data stored by the conversions backend.
   * onReportsChanged and onSourcesChanged will be called
   * automatically as data is deleted, so there's no need to manually refresh
   * the data on completion.
   */
  clearStorage(): void {
    this.sourceRegistrations.clearRows();
    this.triggers.clearRows();
    this.eventLevelReports.clearRows(report => !report.isPending());
    this.aggregatableReports.clearRows(report => !report.isPending());
    this.debugReports.clearRows();
    this.osRegistrations.clearRows();
    this.handler.clearStorage();
  }

  onDebugModeChanged(debugMode: boolean): void {
    const reportDelaysContent =
        document.querySelector<HTMLElement>('#report-delays')!;
    const noiseContent = document.querySelector<HTMLElement>('#noise')!;

    if (debugMode) {
      reportDelaysContent.innerText = 'disabled';
      noiseContent.innerText = 'disabled';
    } else {
      reportDelaysContent.innerText = 'enabled';
      noiseContent.innerText = 'enabled';
    }
  }

  refresh(): void {
    this.handler.isAttributionReportingEnabled().then((response) => {
      const featureStatus =
          document.querySelector<HTMLElement>('#feature-status')!;
      featureStatus.innerText = response.enabled ? 'enabled' : 'disabled';

      const attributionSupport = document.querySelector<HTMLElement>('#attribution-support')!;
      attributionSupport.innerText =
          attributionSupportText[response.attributionSupport];
    });
  }

  onSourcesChanged(sources: WebUISource[]): void {
    this.sources.updateRows(function*() {
      for (const source of sources) {
        yield newSource(source);
      }
    }());
  }

  onReportsChanged(reports: WebUIReport[]): void {
    this.eventLevelReports.updateRows(function*() {
      for (const report of reports) {
        if (report.data.eventLevelData !== undefined) {
          yield new EventLevelReport(report);
        }
      }
    }());

    this.aggregatableReports.updateRows(function*() {
      for (const report of reports) {
        if (report.data.aggregatableAttributionData !== undefined) {
          yield new AggregatableReport(report);
        }
      }
    }());
  }
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

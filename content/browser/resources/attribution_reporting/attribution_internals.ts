// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './attribution_internals_table.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {TriggerAttestation} from './attribution.mojom-webui.js';
import {Factory, HandlerInterface, HandlerRemote, ObserverInterface, ObserverReceiver, ReportID, SourceStatus, WebUIDebugReport, WebUIOsRegistration, WebUIRegistration, WebUIReport, WebUISource, WebUISource_Attributability, WebUISourceRegistration, WebUITrigger, WebUITrigger_Status} from './attribution_internals.mojom-webui.js';
import {AttributionInternalsTableElement} from './attribution_internals_table.js';
import {OsRegistrationResult, OsRegistrationType} from './attribution_reporting.mojom-webui.js';
import {SourceRegistrationError} from './source_registration_error.mojom-webui.js';
import {SourceType} from './source_type.mojom-webui.js';
import {StoreSourceResult} from './store_source_result.mojom-webui.js';
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
  readonly compare?: (a: T, b: T) => number;

  constructor(
      private readonly header: string,
      protected readonly getValue: (param: T) => V,
      comparable: boolean = true) {
    if (comparable) {
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
    super(header, getValue, /*comparable=*/ false);
  }

  override render(td: HTMLElement, row: T) {
    const code = td.ownerDocument.createElement('code');
    code.innerText = this.getValue(row);

    const pre = td.ownerDocument.createElement('pre');
    pre.appendChild(code);

    td.appendChild(pre);
  }
}

class ListColumn<T, V> extends ValueColumn<T, V[]> {
  constructor(
      header: string, getValue: (p: T) => V[],
      private readonly flatten: boolean = false) {
    super(header, getValue, /*comparable=*/ false);
  }

  override render(td: HTMLElement, row: T) {
    const values = this.getValue(row);
    if (values.length === 0) {
      return;
    }

    if (this.flatten && values.length === 1) {
      td.innerText = `${values[0]}`;
      return;
    }

    const ul = td.ownerDocument.createElement('ul');

    values.forEach(value => {
      const li = td.ownerDocument.createElement('li');
      li.innerText = `${value}`;
      ul.appendChild(li);
    });

    td.appendChild(ul);
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

const debugPathPattern: RegExp =
    /(?<=\/\.well-known\/attribution-reporting\/)debug(?=\/)/;

class ReportUrlColumn<T extends Report> extends ValueColumn<T, string> {
  constructor() {
    super('Report URL', (e) => e.reportUrl);
  }

  override render(td: HTMLElement, row: T) {
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
  private readonly selectAll: HTMLInputElement;
  private readonly listener: () => void;
  readonly selectionChangedListeners: Set<(param: boolean) => void> = new Set();

  constructor(private readonly model: TableModel<T>) {
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
  destinations: string[];
  reportingOrigin: string;
  sourceTime: Date;
  expiryTime: Date;
  eventReportWindowTime: Date;
  aggregatableReportWindowTime: Date;
  sourceType: string;
  filterData: string;
  aggregationKeys: string;
  debugKey: string;
  dedupKeys: bigint[];
  priority: bigint;
  status: string;
  aggregatableBudgetConsumed: bigint;
  aggregatableDedupKeys: bigint[];

  constructor(mojo: WebUISource) {
    this.sourceEventId = mojo.sourceEventId;
    this.sourceOrigin = originToText(mojo.sourceOrigin);
    this.destinations =
        mojo.destinations.destinations.map(d => originToText(d.siteAsOrigin));
    this.reportingOrigin = originToText(mojo.reportingOrigin);
    this.sourceTime = new Date(mojo.sourceTime);
    this.expiryTime = new Date(mojo.expiryTime);
    this.eventReportWindowTime = new Date(mojo.eventReportWindowTime);
    this.aggregatableReportWindowTime =
        new Date(mojo.aggregatableReportWindowTime);
    this.sourceType = sourceTypeToText(mojo.sourceType);
    this.priority = mojo.priority;
    this.filterData = JSON.stringify(mojo.filterData, null, ' ');
    this.aggregationKeys =
        JSON.stringify(mojo.aggregationKeys, bigintReplacer, ' ');
    this.debugKey = mojo.debugKey ? `${mojo.debugKey.value}` : '';
    this.dedupKeys = mojo.dedupKeys;
    this.aggregatableBudgetConsumed = mojo.aggregatableBudgetConsumed;
    this.aggregatableDedupKeys = mojo.aggregatableDedupKeys;
    this.status = attributabilityToText(mojo.attributability);
  }
}

class SourceTableModel extends TableModel<Source> {
  private storedSources: Source[] = [];

  constructor() {
    super(
        [
          new ValueColumn<Source, bigint>(
              'Source Event ID', (e) => e.sourceEventId),
          new ValueColumn<Source, string>('Status', (e) => e.status),
          new ValueColumn<Source, string>(
              'Source Origin', (e) => e.sourceOrigin),
          new ListColumn<Source, string>(
              'Destinations', (e) => e.destinations, /*flatten=*/ true),
          new ValueColumn<Source, string>(
              'Reporting Origin', (e) => e.reportingOrigin),
          new DateColumn<Source>(
              'Source Registration Time', (e) => e.sourceTime),
          new DateColumn<Source>('Expiry Time', (e) => e.expiryTime),
          new DateColumn<Source>(
              'Event Report Window Time', (e) => e.eventReportWindowTime),
          new DateColumn<Source>(
              'Aggregatable Report Window Time',
              (e) => e.aggregatableReportWindowTime),
          new ValueColumn<Source, string>('Source Type', (e) => e.sourceType),
          new ValueColumn<Source, bigint>('Priority', (e) => e.priority),
          new CodeColumn<Source>('Filter Data', (e) => e.filterData),
          new CodeColumn<Source>('Aggregation Keys', (e) => e.aggregationKeys),
          new ValueColumn<Source, string>(
              'Aggregatable Budget Consumed',
              (e) => `${e.aggregatableBudgetConsumed} / ${BUDGET_PER_SOURCE}`),
          new ValueColumn<Source, string>('Debug Key', (e) => e.debugKey),
          new ListColumn<Source, bigint>('Dedup Keys', (e) => e.dedupKeys),
          new ListColumn<Source, bigint>(
              'Aggregatable Dedup Keys', (e) => e.aggregatableDedupKeys),
        ],
        5,  // Sort by source registration time by default.
        'No sources.',
    );
  }

  override getRows() {
    return this.storedSources;
  }

  setStoredSources(storedSources: Source[]) {
    this.storedSources = storedSources;
    this.notifyRowsChanged();
  }

  clear() {
    this.storedSources = [];
    this.notifyRowsChanged();
  }
}

class Registration {
  readonly time: Date;
  readonly contextOrigin: string;
  readonly reportingOrigin: string;
  readonly registrationJson: string;
  readonly clearedDebugKey: string;

  constructor(mojo: WebUIRegistration) {
    this.time = new Date(mojo.time);
    this.contextOrigin = originToText(mojo.contextOrigin);
    this.reportingOrigin = originToText(mojo.reportingOrigin);
    this.registrationJson = mojo.registrationJson;
    this.clearedDebugKey =
        mojo.clearedDebugKey ? `${mojo.clearedDebugKey.value}` : '';
  }
}

function registrationTableColumns<T extends Registration>(
    contextOriginTitle: string): Array<Column<T>> {
  return [
    new DateColumn<T>('Time', (e) => e.time),
    new ValueColumn<T, string>(contextOriginTitle, (e) => e.contextOrigin),
    new ValueColumn<T, string>('Reporting Origin', (e) => e.reportingOrigin),
    new CodeColumn<T>('Registration JSON', (e) => e.registrationJson),
    new ValueColumn<T, string>('Cleared Debug Key', (e) => e.clearedDebugKey),
  ];
}

class RegistrationTableModel<T extends Registration> extends TableModel<T> {
  private registrations: T[] = [];

  constructor(contextOriginTitle: string, cols: Array<Column<T>>) {
    super(
        registrationTableColumns<T>(contextOriginTitle).concat(cols),
        0,  // Sort by time by default.
        'No registrations.',
    );
  }

  override getRows() {
    return this.registrations;
  }

  addRegistration(registration: T) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.registrations.length >= 1000) {
      this.registrations = [];
    }

    this.registrations.push(registration);
    this.notifyRowsChanged();
  }

  clear() {
    this.registrations = [];
    this.notifyRowsChanged();
  }
}


class Trigger extends Registration {
  readonly eventLevelStatus: string;
  readonly aggregatableStatus: string;
  readonly attestation?: TriggerAttestation;

  constructor(mojo: WebUITrigger) {
    super(mojo.registration);
    this.eventLevelStatus = triggerStatusToText(mojo.eventLevelStatus);
    this.aggregatableStatus = triggerStatusToText(mojo.aggregatableStatus);
    this.attestation = mojo.attestation;
  }
}

const ATTESTATION_COLS: Array<Column<TriggerAttestation>> = [
  new ValueColumn<TriggerAttestation, string>('Token', e => e.token),
  new ValueColumn<TriggerAttestation, string>(
      'Report ID', e => e.aggregatableReportId),
];

class AttestationColumn implements Column<Trigger> {
  renderHeader(th: HTMLElement) {
    th.innerText = 'Attestation';
  }

  render(td: HTMLElement, row: Trigger) {
    if (row.attestation) {
      renderDL(td, row.attestation, ATTESTATION_COLS);
    }
  }
}

class TriggerTableModel extends RegistrationTableModel<Trigger> {
  constructor() {
    super('Destination', [
      new ValueColumn<Trigger, string>(
          'Event-Level Status', (e) => e.eventLevelStatus),
      new ValueColumn<Trigger, string>(
          'Aggregatable Status', (e) => e.aggregatableStatus),
      new AttestationColumn(),
    ]);
  }
}

class SourceRegistration extends Registration {
  readonly type: string;
  readonly status: string;

  constructor(mojo: WebUISourceRegistration) {
    super(mojo.registration);
    this.type = sourceTypeToText(mojo.type);
    this.status = sourceRegistrationStatusToText(mojo.status);
  }
}

class SourceRegistrationTableModel extends
    RegistrationTableModel<SourceRegistration> {
  constructor() {
    super('Source Origin', [
      new ValueColumn<SourceRegistration, string>('Type', (e) => e.type),
      new ValueColumn<SourceRegistration, string>('Status', (e) => e.status),
    ]);
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

    this.isDebug = this.reportUrl.indexOf(
                       '/.well-known/attribution-reporting/debug/') >= 0;

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
  attestationToken: string;
  aggregationCoordinator: string;

  constructor(mojo: WebUIReport) {
    super(mojo);

    this.contributions = JSON.stringify(
        mojo.data.aggregatableAttributionData!.contributions, bigintReplacer,
        ' ');

    this.attestationToken =
        mojo.data.aggregatableAttributionData!.attestationToken || '';

    this.aggregationCoordinator =
        mojo.data.aggregatableAttributionData!.aggregationCoordinator;
  }
}

function commonReportTableColumns<T extends Report>(): Array<Column<T>> {
  return [
    new CodeColumn<T>('Report Body', (e) => e.reportBody),
    new ValueColumn<T, string>('Status', (e) => e.status),
    new ReportUrlColumn<T>(),
    new DateColumn<T>('Trigger Time', (e) => e.triggerTime),
    new DateColumn<T>('Report Time', (e) => e.reportTime),
  ];
}

class ReportTableModel<T extends Report> extends TableModel<T> {
  private readonly showDebugReportsCheckbox: HTMLInputElement;
  private readonly hiddenDebugReportsSpan: HTMLSpanElement;
  private sentOrDroppedReports: T[] = [];
  private storedReports: T[] = [];
  private debugReports: T[] = [];

  constructor(
      cols: Array<Column<T>>, showDebugReportsContainer: HTMLElement,
      private readonly sendReportsButton: HTMLButtonElement,
      private readonly handler: HandlerInterface) {
    super(
        commonReportTableColumns<T>().concat(cols),
        5,  // Sort by report time by default; the extra column is added below
        'No sent or pending reports.',
    );

    // This can't be included in the super call above, as `this` can't be
    // accessed until after `super` returns.
    const selectionColumn = new SelectionColumn<T>(this);
    this.cols.unshift(selectionColumn);

    this.showDebugReportsCheckbox =
        showDebugReportsContainer.querySelector<HTMLInputElement>(
            'input[type="checkbox"]')!;

    this.hiddenDebugReportsSpan =
        showDebugReportsContainer.querySelector('span')!;

    this.showDebugReportsCheckbox.addEventListener(
        'input', () => this.notifyRowsChanged());

    this.sendReportsButton.addEventListener('click', () => this.sendReports_());
    selectionColumn.selectionChangedListeners.add((anySelected: boolean) => {
      this.sendReportsButton.disabled = !anySelected;
    });

    this.rowsChangedListeners.add(() => this.updateHiddenDebugReportsSpan_());
  }

  override styleRow(tr: HTMLElement, report: Report) {
    tr.classList.toggle('send-error', report.sendFailed);
  }

  override getRows() {
    let rows = this.sentOrDroppedReports.concat(this.storedReports);
    if (this.showDebugReportsCheckbox.checked) {
      rows = rows.concat(this.debugReports);
    }
    return rows;
  }

  setStoredReports(storedReports: T[]) {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  addSentOrDroppedReport(report: T) {
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

    this.handler.sendReports(ids).then(() => {
      this.sendReportsButton.innerText = previousText;
    });
  }
}

class EventLevelReportTableModel extends ReportTableModel<EventLevelReport> {
  constructor(
      showDebugReportsContainer: HTMLElement,
      sendReportsButton: HTMLButtonElement, remote: HandlerInterface) {
    super(
        [
          new ValueColumn<EventLevelReport, bigint>(
              'Report Priority', (e) => e.reportPriority),
          new ValueColumn<EventLevelReport, string>(
              'Randomized Report',
              (e) => e.attributedTruthfully ? 'no' : 'yes'),
        ],
        showDebugReportsContainer,
        sendReportsButton,
        remote,
    );
  }
}

class AggregatableAttributionReportTableModel extends
    ReportTableModel<AggregatableAttributionReport> {
  constructor(
      showDebugReportsContainer: HTMLElement,
      sendReportsButton: HTMLButtonElement, remote: HandlerInterface) {
    super(
        [
          new CodeColumn<AggregatableAttributionReport>(
              'Histograms', (e) => e.contributions),
          new ValueColumn<AggregatableAttributionReport, string>(
              'Attestation Token', (e) => e.attestationToken),
          new ValueColumn<AggregatableAttributionReport, string>(
            'Aggregation Coordinator', (e) => e.aggregationCoordinator),
        ],
        showDebugReportsContainer,
        sendReportsButton,
        remote,
    );
  }
}

class OsRegistration {
  timestamp: Date;
  registrationUrl: string;
  topLevelOrigin: string;
  registrationType: string;
  debugKeyAllowed: boolean;
  result: string;

  constructor(mojo: WebUIOsRegistration) {
    this.timestamp = new Date(mojo.time);
    this.registrationUrl = mojo.registrationUrl.url;
    this.topLevelOrigin = originToText(mojo.topLevelOrigin);
    this.debugKeyAllowed = mojo.isDebugKeyAllowed;

    switch (mojo.type) {
      case OsRegistrationType.kSource:
        this.registrationType = 'OS Source';
        break;
      case OsRegistrationType.kTrigger:
        this.registrationType = 'OS Trigger';
        break;
      default:
        assertNotReached();
    }

    switch (mojo.result) {
      case OsRegistrationResult.kPassedToOs:
        this.result = 'Passed to OS';
        break;
      case OsRegistrationResult.kUnsupported:
        this.result = 'Unsupported';
        break;
      case OsRegistrationResult.kInvalidRegistrationUrl:
        this.result = 'Invalid registration URL';
        break;
      case OsRegistrationResult.kProhibitedByBrowserPolicy:
        this.result = 'Prohibited by browser policy';
        break;
      case OsRegistrationResult.kExcessiveQueueSize:
        this.result = 'Excessive queue size';
        break;
      case OsRegistrationResult.kRejectedByOs:
        this.result = 'Rejected by OS';
        break;
      default:
        assertNotReached();
    }
  }
}

class OsRegistrationTableModel extends TableModel<OsRegistration> {
  private osRegistrations: OsRegistration[] = [];

  constructor() {
    super(
        [
          new DateColumn<OsRegistration>('Timestamp', (e) => e.timestamp),
          new ValueColumn<OsRegistration, string>(
              'Registration Type', (e) => e.registrationType),
          new ValueColumn<OsRegistration, string>(
              'Registration URL', (e) => e.registrationUrl),
          new ValueColumn<OsRegistration, string>(
              'Top-Level Origin', (e) => e.topLevelOrigin),
          new ValueColumn<OsRegistration, boolean>(
              'Debug Key Allowed', (e) => e.debugKeyAllowed),
          new ValueColumn<OsRegistration, string>('Result', (e) => e.result),
        ],
        0,
        'No OS Registrations',
    );
  }

  override getRows() {
    return this.osRegistrations;
  }

  addOsRegistration(osRegistration: OsRegistration) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.osRegistrations.length >= 1000) {
      this.osRegistrations = [];
    }

    this.osRegistrations.push(osRegistration);
    this.notifyRowsChanged();
  }

  clear() {
    this.osRegistrations = [];
    this.notifyRowsChanged();
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

class DebugReportTableModel extends TableModel<DebugReport> {
  private debugReports: DebugReport[] = [];

  constructor() {
    super(
        [
          new DateColumn<DebugReport>('Time', (e) => e.time),
          new ValueColumn<DebugReport, string>('URL', (e) => e.url),
          new ValueColumn<DebugReport, string>('Status', (e) => e.status),
          new CodeColumn<DebugReport>('Body', (e) => e.body),
        ],
        0,  // Sort by report time by default.
        'No verbose debug reports.',
    );
  }

  // TODO(apaseltiner): Style error rows like `ReportTableModel`

  override getRows() {
    return this.debugReports;
  }

  add(report: DebugReport) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.debugReports.length >= 1000) {
      this.debugReports = [];
    }

    this.debugReports.push(report);
    this.notifyRowsChanged();
  }

  clear() {
    this.debugReports = [];
    this.notifyRowsChanged();
  }
}

function sourceRegistrationErrorToText(error: SourceRegistrationError) {
  switch (error) {
    case SourceRegistrationError.kInvalidJson:
      return 'invalid syntax';
    case SourceRegistrationError.kRootWrongType:
      return 'root JSON value has wrong type (must be a dictionary)';
    case SourceRegistrationError.kDestinationMissing:
      return 'destination missing';
    case SourceRegistrationError.kDestinationWrongType:
      return 'destination has wrong type (must be a string)';
    case SourceRegistrationError.kDestinationListTooLong:
      return 'number of destinations exceeds limit';
    case SourceRegistrationError.kDestinationUntrustworthy:
      return 'destination not potentially trustworthy';
    case SourceRegistrationError.kFilterDataWrongType:
      return 'filter_data has wrong type (must be a dictionary)';
    case SourceRegistrationError.kFilterDataTooManyKeys:
      return 'filter_data has too many keys';
    case SourceRegistrationError.kFilterDataHasSourceTypeKey:
      return 'filter_data must not have a source_type key';
    case SourceRegistrationError.kFilterDataKeyTooLong:
      return 'filter_data key too long';
    case SourceRegistrationError.kFilterDataListWrongType:
      return 'filter_data value has wrong type (must be a list)';
    case SourceRegistrationError.kFilterDataListTooLong:
      return 'filter_data list too long';
    case SourceRegistrationError.kFilterDataValueWrongType:
      return 'filter_data list value has wrong type (must be a string)';
    case SourceRegistrationError.kFilterDataValueTooLong:
      return 'filter_data list value too long';
    case SourceRegistrationError.kAggregationKeysWrongType:
      return 'aggregation_keys has wrong type (must be a dictionary)';
    case SourceRegistrationError.kAggregationKeysTooManyKeys:
      return 'aggregation_keys has too many keys';
    case SourceRegistrationError.kAggregationKeysKeyTooLong:
      return 'aggregation_keys key too long';
    case SourceRegistrationError.kAggregationKeysValueWrongType:
      return 'aggregation_keys value has wrong type (must be a string)';
    case SourceRegistrationError.kAggregationKeysValueWrongFormat:
      return 'aggregation_keys value must be a base-16 integer starting with 0x';
    default:
      return 'unknown error';
  }
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
      assertNotReached();
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
    case WebUISource_Attributability.kNoisedNever:
      return 'Unattributable: noised with no reports';
    case WebUISource_Attributability.kNoisedFalsely:
      return 'Unattributable: noised with fake reports';
    case WebUISource_Attributability.kReachedEventLevelAttributionLimit:
      return 'Attributable: reached event-level attribution limit';
    default:
      assertNotReached();
  }
}

function sourceRegistrationStatusToText(status: SourceStatus): string {
  if (status.storeSourceResult !== undefined) {
    switch (status.storeSourceResult) {
      case StoreSourceResult.kSuccess:
      case StoreSourceResult.kSuccessNoised:
        return 'Success';
      case StoreSourceResult.kInternalError:
        return 'Rejected: internal error';
      case StoreSourceResult.kInsufficientSourceCapacity:
        return 'Rejected: insufficient source capacity';
      case StoreSourceResult.kInsufficientUniqueDestinationCapacity:
        return 'Rejected: insufficient unique destination capacity';
      case StoreSourceResult.kExcessiveReportingOrigins:
        return 'Rejected: excessive reporting origins';
      case StoreSourceResult.kProhibitedByBrowserPolicy:
        return 'Rejected: prohibited by browser policy';
      default:
        return status.toString();
    }
  } else if (status.jsonError !== undefined) {
    return `Rejected: invalid JSON: ${
        sourceRegistrationErrorToText(status.jsonError)}`;
  } else {
    return 'Unknown';
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
    case WebUITrigger_Status.kReportWindowPassed:
      return 'Failure: Report window has passed';
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
      assertNotReached();
  }
}

class AttributionInternals implements ObserverInterface {
  private readonly sources = new SourceTableModel();
  private readonly sourceRegistrations = new SourceRegistrationTableModel();
  private readonly triggers = new TriggerTableModel();
  private readonly debugReports = new DebugReportTableModel();
  private readonly osRegistrations = new OsRegistrationTableModel();
  private readonly eventLevelReports: EventLevelReportTableModel;
  private readonly aggregatableReports: AggregatableAttributionReportTableModel;

  private readonly handler = new HandlerRemote();

  constructor() {
    this.eventLevelReports = new EventLevelReportTableModel(
        document.querySelector<HTMLButtonElement>('#show-debug-event-reports')!,
        document.querySelector<HTMLButtonElement>('#send-reports')!,
        this.handler);

    this.aggregatableReports = new AggregatableAttributionReportTableModel(
        document.querySelector<HTMLButtonElement>(
            '#show-debug-aggregatable-reports')!,
        document.querySelector<HTMLButtonElement>('#send-aggregatable-reports')!
        ,
        this.handler);

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

    Factory.getRemote().create(
        new ObserverReceiver(this).$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  onSourcesChanged() {
    this.updateSources();
  }

  onReportsChanged() {
    this.updateReports();
  }

  onReportSent(mojo: WebUIReport) {
    this.addSentOrDroppedReport(mojo);
  }

  onDebugReportSent(mojo: WebUIDebugReport) {
    this.debugReports.add(new DebugReport(mojo));
  }

  onReportDropped(mojo: WebUIReport) {
    this.addSentOrDroppedReport(mojo);
  }

  onSourceHandled(mojo: WebUISourceRegistration) {
    this.sourceRegistrations.addRegistration(new SourceRegistration(mojo));
  }

  onTriggerHandled(mojo: WebUITrigger) {
    this.triggers.addRegistration(new Trigger(mojo));
  }

  onOsRegistration(mojo: WebUIOsRegistration) {
    this.osRegistrations.addOsRegistration(new OsRegistration(mojo));
  }

  private addSentOrDroppedReport(mojo: WebUIReport) {
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
  clearStorage() {
    this.sources.clear();
    this.sourceRegistrations.clear();
    this.triggers.clear();
    this.eventLevelReports.clear();
    this.aggregatableReports.clear();
    this.debugReports.clear();
    this.osRegistrations.clear();
    this.handler.clearStorage();
  }

  refresh() {
    this.handler.isAttributionReportingEnabled().then((response) => {
      const featureStatusContent =
          document.querySelector<HTMLElement>('#feature-status-content')!;
      featureStatusContent.innerText =
          response.enabled ? 'enabled' : 'disabled';
      featureStatusContent.classList.toggle('disabled', !response.enabled);

      const debugModeContent =
          document.querySelector<HTMLElement>('#debug-mode-content')!;
      const html = getTrustedHTML`The #attribution-reporting-debug-mode flag is
 <strong>enabled</strong>, reports are sent immediately and never pending.`;
      debugModeContent.innerHTML = html as unknown as string;

      if (!response.debugMode) {
        debugModeContent.innerText = '';
      }

      const attributionSupport = document.querySelector<HTMLElement>('#attribution-support')!;
      attributionSupport.innerText = response.attributionSupport;
    });

    this.updateSources();
    this.updateReports();
  }

  private updateSources() {
    this.handler.getActiveSources().then((response) => {
      this.sources.setStoredSources(
          response.sources.map((mojo) => new Source(mojo)));
    });
  }

  private updateReports() {
    this.handler.getReports().then(response => {
      const eventLevelReports: EventLevelReport[] = [];
      const aggregatableReports: AggregatableAttributionReport[] = [];

      response.reports.forEach(report => {
        if (report.data.eventLevelData !== undefined) {
          eventLevelReports.push(new EventLevelReport(report));
        } else if (report.data.aggregatableAttributionData !== undefined) {
          aggregatableReports.push(new AggregatableAttributionReport(report));
        }
      });

      this.eventLevelReports.setStoredReports(eventLevelReports);
      this.aggregatableReports.setStoredReports(aggregatableReports);
    });
  }
}

function installUnreadIndicator(model: TableModel<any>, tab: HTMLElement) {
  model.rowsChangedListeners.add(() => {
    if (!tab.hasAttribute('selected')) {
      tab.classList.add('unread');
    }
  });
}

document.addEventListener('DOMContentLoaded', function() {
  const tabBox = document.querySelector('cr-tab-box')!;
  tabBox.addEventListener('selected-index-change', e => {
    const tabs = document.querySelectorAll<HTMLElement>('div[slot=\'tab\']');
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

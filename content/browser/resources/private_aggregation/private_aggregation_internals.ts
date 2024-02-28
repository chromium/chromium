// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './private_aggregation_internals_table.js';

import {assert} from 'chrome://resources/js/assert.js';

import type {AggregatableReportRequestID, ObserverInterface, WebUIAggregatableReport} from './private_aggregation_internals.mojom-webui.js';
import {Factory as PrivateAggregationInternalsFactory, HandlerRemote as PrivateAggregationInternalsHandlerRemote, ObserverReceiver, ReportStatus} from './private_aggregation_internals.mojom-webui.js';
import type {PrivateAggregationInternalsTableElement} from './private_aggregation_internals_table.js';
import type {Column} from './table_model.js';
import {TableModel} from './table_model.js';

function compareDefault<T>(a: T, b: T): number {
  return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

// Converts the mojo_base.mojom.Uint128 to a string
function bucketReplacer(_key: string, value: any): any {
  if (_key === 'bucket') {
    return (value['high'] * 2n ** 64n + value['low']).toString();
  } else {
    return value;
  }
}

class ValueColumn<T, V> implements Column<T> {
  compare: (a: T, b: T) => number;
  header: string;
  private minWidth?: string;
  protected getValue: (param: T) => V;

  constructor(
      header: string, getValue: (param: T) => V, minWidth?: string,
      compare?: ((a: T, b: T) => number)) {
    this.header = header;
    this.getValue = getValue;
    this.minWidth = minWidth;
    this.compare =
        compare ?? ((a: T, b: T) => compareDefault(getValue(a), getValue(b)));
  }

  render(td: HTMLElement, row: T) {
    if (this.minWidth) {
      td.style.minWidth = this.minWidth;
    }
    td.textContent = `${this.getValue(row)}`;
  }

  renderHeader(th: HTMLElement) {
    th.textContent = `${this.header}`;
  }
}

/**
 * Column that holds date.
 */
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

/**
 * Wraps a checkbox.
 */
class Selectable {
  selectCheckbox: HTMLInputElement;

  constructor() {
    this.selectCheckbox = document.createElement('input');
    this.selectCheckbox.type = 'checkbox';
  }
}

/**
 * Checkbox column for selection.
 */
class SelectionColumn<T extends Selectable> implements Column<T> {
  compare: ((a: T, b: T) => number)|null;
  private model: TableModel<T>;
  private selectAllCheckbox: HTMLInputElement;
  selectionChangedListeners: Set<(param: boolean) => void>;
  header: string|null;
  private rowChangedListener: () => void;

  constructor(model: TableModel<T>) {
    // Selection column is not sortable.
    this.compare = null;
    this.model = model;
    this.header = null;

    this.selectAllCheckbox = document.createElement('input');
    this.selectAllCheckbox.type = 'checkbox';
    this.selectAllCheckbox.addEventListener('input', () => {
      const checked = this.selectAllCheckbox.checked;
      this.model.getRows().forEach((row) => {
        if (!row.selectCheckbox.disabled) {
          row.selectCheckbox.checked = checked;
        }
      });
      this.notifySelectionChanged(checked);
    });

    this.rowChangedListener = () => this.onChange();
    this.model.rowsChangedListeners.add(this.rowChangedListener);
    this.selectionChangedListeners = new Set();
  }

  render(td: HTMLElement, row: T) {
    td.appendChild(row.selectCheckbox);
  }

  renderHeader(th: HTMLElement) {
    th.appendChild(this.selectAllCheckbox);
  }

  onChange() {
    let anySelectable = false;
    let anySelected = false;
    let anyUnselected = false;

    this.model.getRows().forEach((row) => {
      // addEventListener deduplicates, so only one event will be fired per
      // input.
      row.selectCheckbox.addEventListener('input', this.rowChangedListener);

      if (row.selectCheckbox.disabled) {
        return;
      }

      anySelectable = true;
      if (row.selectCheckbox.checked) {
        anySelected = true;
      } else {
        anyUnselected = true;
      }
    });

    this.selectAllCheckbox.disabled = !anySelectable;
    this.selectAllCheckbox.checked = anySelected && !anyUnselected;
    this.selectAllCheckbox.indeterminate = anySelected && anyUnselected;

    this.notifySelectionChanged(anySelected);
  }

  notifySelectionChanged(anySelected: boolean) {
    this.selectionChangedListeners.forEach((f) => f(anySelected));
  }
}

function reportStatusToText(status: ReportStatus) {
  switch (status) {
    case ReportStatus.kPending:
      return 'Pending';
    case ReportStatus.kSent:
      return 'Sent';
    case ReportStatus.kFailedToAssemble:
      return 'Failed to assemble';
    case ReportStatus.kFailedToSend:
      return 'Failed to send';
  }
}

class Report extends Selectable {
  // `null` indicates a report that wasn't stored/scheduled.
  id: AggregatableReportRequestID|null;
  reportBody: string;
  reportUrl: string;
  reportTime: Date;
  status: string;
  apiIdentifier: string;
  apiVersion: string;
  contributions: string;

  constructor(mojo: WebUIAggregatableReport) {
    super();

    this.id = mojo.id;
    this.reportBody = mojo.reportBody;
    this.reportUrl = mojo.reportUrl.url;
    this.reportTime = new Date(mojo.reportTime);
    this.apiIdentifier = mojo.apiIdentifier;
    this.apiVersion = mojo.apiVersion;

    // Only pending stored/scheduled reports are selectable.
    if (mojo.status !== ReportStatus.kPending || mojo.id === undefined) {
      this.selectCheckbox.disabled = true;
    }

    this.status = reportStatusToText(mojo.status);

    this.contributions =
        JSON.stringify(mojo.contributions, bucketReplacer, ' ');
  }
}

class ReportTableModel extends TableModel<Report> {
  private sendReportsButton: HTMLButtonElement;
  private selectionColumn: SelectionColumn<Report>;
  private handledReports: Report[] = [];
  private storedReports: Report[] = [];

  constructor(sendReportsButton: HTMLButtonElement) {
    super();

    this.sendReportsButton = sendReportsButton;

    this.selectionColumn = new SelectionColumn(this);

    this.cols = [
      this.selectionColumn,
      new ValueColumn<Report, string>('Status', (e) => e.status),
      new ValueColumn<Report, string>(
          'Report URL', (e) => e.reportUrl, '250px'),
      new DateColumn<Report>('Report Time', (e) => e.reportTime),
      new ValueColumn<Report, string>(
          'API identifier', (e) => e.apiIdentifier, '90px'),
      new ValueColumn<Report, string>('API version', (e) => e.apiVersion),
      new CodeColumn<Report>(
          'Contributions', (e) => (e as Report).contributions),
      new CodeColumn<Report>('Report Body', (e) => e.reportBody),
    ];

    // Sort by report time by default.
    this.sortIdx = 3;
    assert(this.cols[this.sortIdx]!.header === 'Report Time');

    this.emptyRowText = 'No sent or pending reports.';

    this.sendReportsButton.addEventListener('click', () => this.sendReports_());
    this.selectionColumn.selectionChangedListeners.add(
        (anySelected: boolean) => {
          this.sendReportsButton.disabled = !anySelected;
        });
  }

  override getRows() {
    return this.handledReports.concat(this.storedReports);
  }

  setStoredReports(storedReports: Report[]) {
    this.storedReports = storedReports;
    this.notifyRowsChanged();
  }

  addHandledReport(report: Report) {
    // Prevent the page from consuming ever more memory if the user leaves the
    // page open for a long time.
    if (this.handledReports.length >= 1000) {
      this.handledReports = [];
    }

    this.handledReports.push(report);

    this.notifyRowsChanged();
  }

  clear() {
    this.storedReports = [];
    this.handledReports = [];
    this.notifyRowsChanged();
  }

  /**
   * Sends all selected reports.
   * Disables the button while the reports are still being sent.
   * Observer.onRequestStorageModified will be called automatically as reports
   * are deleted, so there's no need to manually refresh the data on completion.
   */
  private sendReports_() {
    const ids: AggregatableReportRequestID[] = [];
    this.storedReports.forEach((report) => {
      if (!report.selectCheckbox.disabled && report.selectCheckbox.checked) {
        ids.push(report.id as AggregatableReportRequestID);
      }
    });

    if (ids.length === 0) {
      return;
    }

    const previousText = this.sendReportsButton.innerText;

    this.sendReportsButton.disabled = true;
    this.sendReportsButton.innerText = 'Sending...';

    pageHandler!.sendReports(ids).then(() => {
      this.sendReportsButton.innerText = previousText;
    });
  }
}

/**
 * Reference to the backend providing all the data.
 */
let pageHandler: PrivateAggregationInternalsHandlerRemote|null = null;

let reportTableModel: ReportTableModel|null = null;

/**
 * Fetches all pending reports from the backend and populate the tables.
 */
function updateReports() {
  pageHandler!.getReports().then((response) => {
    reportTableModel!.setStoredReports(
        response.reports.map((mojo) => new Report(mojo)));
  });
}

/**
 * Deletes all data stored by the aggregation service backend.
 * Observer.onRequestStorageModified will be called automatically as reports are
 * deleted, so there's no need to manually refresh the data on completion.
 */
function clearStorage() {
  reportTableModel!.clear();
  pageHandler!.clearStorage();
}

class Observer implements ObserverInterface {
  onRequestStorageModified() {
    updateReports();
  }

  onReportHandled(mojo: WebUIAggregatableReport) {
    reportTableModel!.addHandledReport(new Report(mojo));
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Setup the mojo interface.
  pageHandler = new PrivateAggregationInternalsHandlerRemote();

  const sendReports =
      document.querySelector<HTMLButtonElement>('#send-reports');
  reportTableModel = new ReportTableModel(sendReports!);

  const refresh = document.querySelector('#refresh');
  refresh!.addEventListener('click', updateReports);
  const clearData = document.querySelector('#clear-data');
  clearData!.addEventListener('click', clearStorage);

  const reportTable =
      document.querySelector<PrivateAggregationInternalsTableElement<Report>>(
          '#reportTable');
  reportTable!.setModel(reportTableModel!);

  PrivateAggregationInternalsFactory.getRemote().create(
    new ObserverReceiver(new Observer()).$.bindNewPipeAndPassRemote(),
    pageHandler.$.bindNewPipeAndPassReceiver());

  updateReports();
});

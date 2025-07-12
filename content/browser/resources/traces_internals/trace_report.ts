// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import './icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Token} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {getCss} from './trace_report.css.js';
import {getHtml} from './trace_report.html.js';
import {Notification, NotificationType} from './trace_report_list.js';
import {downloadTraceData, getTokenAsUuidString} from './trace_util.js';
import {TracesBrowserProxy} from './traces_browser_proxy.js';
import type {ClientTraceReport} from './traces_internals.mojom-webui.js';
import {ReportUploadState, SkipUploadReason} from './traces_internals.mojom-webui.js';

export class TraceReportElement extends CrLitElement {
  static get is() {
    return 'trace-report';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      trace: {type: Object},
      isHeader: {type: Boolean},
      isLoading: {type: Boolean},
    };
  }

  private traceReportProxy_: TracesBrowserProxy =
      TracesBrowserProxy.getInstance();

  protected accessor trace: ClientTraceReport|null = null;
  protected accessor isHeader: boolean = false;
  protected accessor isLoading: boolean = false;

  protected onCopyUuidClick_(): void {
    if (!this.trace) {
      return;
    }
    // Get the text field
    navigator.clipboard.writeText(getTokenAsUuidString(this.trace.uuid));
  }

  protected getTraceSize_(trace: ClientTraceReport): string {
    if (trace.totalSize < 1) {
      return '0 Bytes';
    }

    let displayedSize = Number(trace.totalSize);
    const k = 1024;

    const sizes = ['Bytes', 'KB', 'MB', 'GB'];

    let i = 0;

    for (i; displayedSize >= k && i < 3; i++) {
      displayedSize /= k;
    }

    return `${displayedSize.toFixed(2)} ${sizes[i]}`;
  }

  protected getSkipReason_(trace: ClientTraceReport): string {
    // Keep this in sync with the values of SkipUploadReason in
    // tracereport.mojom
    const skipReasonMap: string[] = [
      'None',
      'Size limit exceeded',
      'Not anonymized',
      'Scenario quota exceeded',
      'Upload timed out',
      'Local scenario',
    ];

    return skipReasonMap[trace.skipReason] ?? 'Could not get the skip reason';
  }

  protected onCopyScenarioClick_(): void {
    if (!this.trace) {
      return;
    }
    // Get the text field
    navigator.clipboard.writeText(this.trace.scenarioName);
  }

  protected onCopyUploadRuleClick_(): void {
    if (!this.trace) {
      return;
    }
    // Get the text field
    navigator.clipboard.writeText(this.trace.uploadRuleName);
  }

  protected isManualUploadDisabled_(trace: ClientTraceReport): boolean {
    return this.isLoading ||
        trace.skipReason === SkipUploadReason.kNotAnonymized;
  }

  protected dateToString_(mojoTime: Time): string {
    // The JS Date() is based off of the number of milliseconds since
    // the UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue|
    // of the base::Time (represented in mojom.Time) represents the
    // number of microseconds since the Windows FILETIME epoch
    // (1601-01-01 00:00:00 UTC). This computes the final JS time by
    // computing the epoch delta and the conversion from microseconds to
    // milliseconds.
    const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
    const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
    // |epochDeltaInMs| equals to
    // base::Time::kTimeTToMicrosecondsOffset.
    const epochDeltaInMs = unixEpoch - windowsEpoch;
    const timeInMs = Number(mojoTime.internalValue) / 1000;

    // Define the format in which the date string is going to be displayed.
    return new Date(timeInMs - epochDeltaInMs)
        .toLocaleString(
            /*locales=*/ undefined, {
              hour: 'numeric',
              minute: 'numeric',
              month: 'short',
              day: 'numeric',
              year: 'numeric',
              hour12: true,
            });
  }

  protected async onDownloadTraceClick_(): Promise<void> {
    if (!this.trace) {
      return;
    }
    this.isLoading = true;
    const {trace} =
        await this.traceReportProxy_.handler.downloadTrace(this.trace.uuid);
    if (trace !== null) {
      this.downloadData_(trace, this.trace.uuid);
    } else {
      this.dispatchToast_(
          `Failed to download trace ${getTokenAsUuidString(this.trace.uuid)}.`);
    }
    this.isLoading = false;
  }

  private downloadData_(data: BigBuffer, uuid: Token): void {
    if (data.invalidBuffer) {
      this.dispatchToast_(
          `Invalid buffer received for ${getTokenAsUuidString(uuid)}.`);
      return;
    }
    try {
      downloadTraceData(data, uuid);
    } catch (e) {
      this.dispatchToast_(`Unable to create blob from trace data for ${
          getTokenAsUuidString(uuid)}.`);
    }
  }

  protected async onDeleteTraceClick_(): Promise<void> {
    if (!this.trace) {
      return;
    }
    this.isLoading = true;
    const {success} =
        await this.traceReportProxy_.handler.deleteSingleTrace(this.trace.uuid);
    if (!success) {
      this.dispatchToast_(
          `Failed to delete ${getTokenAsUuidString(this.trace.uuid)}.`);
    } else {
      this.dispatchReloadRequest_();
    }
  }

  protected async onUploadTraceClick_(): Promise<void> {
    if (!this.trace) {
      return;
    }
    this.isLoading = true;
    const {success} =
        await this.traceReportProxy_.handler.userUploadSingleTrace(
            this.trace.uuid);
    if (!success) {
      this.dispatchToast_(
          `Failed to upload trace ${getTokenAsUuidString(this.trace.uuid)}.`);
    } else {
      this.dispatchReloadRequest_();
    }
    this.isLoading = false;
  }

  protected uploadStateEqual_(
      trace: ClientTraceReport, state: ReportUploadState): boolean {
    return trace.uploadState === state;
  }

  private dispatchToast_(message: string): void {
    this.dispatchEvent(new CustomEvent('show-toast', {
      bubbles: true,
      composed: true,
      detail: new Notification(NotificationType.ERROR, message),
    }));
  }

  protected isDownloadDisabled_(trace: ClientTraceReport): boolean {
    return this.isLoading || !trace.hasTraceContent;
  }

  protected getDownloadTooltip_(trace: ClientTraceReport): string {
    return trace.hasTraceContent ? 'Download Trace' : 'Trace expired';
  }

  private dispatchReloadRequest_(): void {
    this.fire('refresh-traces-request');
  }

  protected getStateCssClass_(trace: ClientTraceReport): string {
    switch (trace.uploadState) {
      case ReportUploadState.kNotUploaded:
        return 'state-default';
      case ReportUploadState.kPending:
      case ReportUploadState.kPending_UserRequested:
        return 'state-pending';
      case ReportUploadState.kUploaded:
        return 'state-success';
      default:
        return '';
    }
  }

  protected getStateText_(trace: ClientTraceReport): string {
    switch (trace.uploadState) {
      case ReportUploadState.kNotUploaded:
        return `Upload skipped: ${this.getSkipReason_(trace)}`;
      case ReportUploadState.kPending:
        return 'Pending upload';
      case ReportUploadState.kPending_UserRequested:
        return 'Pending upload: User requested';
      case ReportUploadState.kUploaded:
        return `Uploaded: ${this.dateToString_(trace.uploadTime)}`;
      default:
        return '';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report': TraceReportElement;
  }
}

customElements.define(TraceReportElement.is, TraceReportElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './icons.html.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {getCss} from './trace_report.css.js';
import {getHtml} from './trace_report.html.js';
import type {ClientTraceReport} from './trace_report.mojom-webui.js';
import {ReportUploadState, SkipUploadReason} from './trace_report.mojom-webui.js';
import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {Notification, NotificationType} from './trace_report_list.js';

// Create the temporary element here to hold the data to download the trace
// since it is only obtained after downloadData_ is called. This way we can
// perform a download directly in JS without touching the element that
// triggers the action. Initiate download a resource identified by |url| into
// |filename|.
function downloadUrl(fileName: string, url: string): void {
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
}

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
      isLoading: {type: Boolean},
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();

  protected trace: ClientTraceReport = {
    // Dummy ClientTraceReport
    uuid: {
      high: 0n,
      low: 0n,
    },
    creationTime: {internalValue: 0n},
    scenarioName: '',
    uploadRuleName: '',
    totalSize: 0n,
    uploadState: ReportUploadState.kNotUploaded,
    uploadTime: {internalValue: 0n},
    skipReason: SkipUploadReason.kNoSkip,
    hasTraceContent: false,
  };
  protected isLoading_: boolean = false;

  protected onCopyUuidClick_(): void {
    // Get the text field
    navigator.clipboard.writeText(this.getTokenAsString_());
  }

  protected getTraceSize_(): string {
    if (this.trace.totalSize < 1) {
      return '0 Bytes';
    }

    let displayedSize = Number(this.trace.totalSize);
    const k = 1024;

    const sizes = ['Bytes', 'KB', 'MB', 'GB'];

    let i = 0;

    for (i; displayedSize >= k && i < 3; i++) {
      displayedSize /= k;
    }

    return `${displayedSize.toFixed(2)} ${sizes[i]}`;
  }

  protected getSkipReason_(): string {
    // Keep this in sync with the values of SkipUploadReason in
    // tracereport.mojom
    const skipReasonMap: string[] = [
      'None',
      'Size limit exceeded',
      'Not anonymized',
      'Scenario quota exceeded',
      'Upload timed out',
    ];

    return skipReasonMap[this.trace.skipReason] ??
        'Could not get the skip reason';
  }

  protected onCopyScenarioClick_(): void {
    // Get the text field
    navigator.clipboard.writeText(this.trace.scenarioName);
  }

  protected onCopyUploadRuleClick_(): void {
    // Get the text field
    navigator.clipboard.writeText(this.trace.uploadRuleName);
  }

  protected isManualUploadPermitted_(): boolean {
    return this.trace.skipReason !== SkipUploadReason.kNotAnonymized;
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
    this.isLoading_ = true;
    const {trace} =
        await this.traceReportProxy_.handler.downloadTrace(this.trace.uuid);
    if (trace !== null) {
      this.downloadData_(`${this.getTokenAsString_()}.gz`, trace);
    } else {
      this.dispatchToast_(`Failed to download trace ${this.getTokenAsString_()}.`);
    }
    this.isLoading_ = false;
  }

  private downloadData_(fileName: string, data: BigBuffer): void {
    if (data.invalidBuffer) {
      this.dispatchToast_(
          `Invalid buffer received for ${this.getTokenAsString_()}.`);
      return;
    }
    try {
      let bytes: Uint8Array;
      if (Array.isArray(data.bytes)) {
        bytes = new Uint8Array(data.bytes);
      } else {
        assert(!!data.sharedMemory, 'sharedMemory must be defined here');
        const sharedMemory = data.sharedMemory!;
        const {buffer, result} =
            sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
        assert(result === Mojo.RESULT_OK, 'Could not map buffer');
        bytes = new Uint8Array(buffer);
      }
      const url = URL.createObjectURL(
          new Blob([bytes], {type: 'application/octet-stream'}));
      downloadUrl(fileName, url);
    } catch (e) {
      this.dispatchToast_(
          `Unable to create blob from trace data for ${this.getTokenAsString_()}.`);
    }
  }

  protected async onDeleteTraceClick_(): Promise<void> {
    this.isLoading_ = true;
    const {success} =
        await this.traceReportProxy_.handler.deleteSingleTrace(this.trace.uuid);
    if (!success) {
      this.dispatchToast_(`Failed to delete ${this.getTokenAsString_()}.`);
    } else {
      this.dispatchReloadRequest_();
    }
    this.isLoading_ = false;
  }

  protected async onUploadTraceClick_(): Promise<void> {
    this.isLoading_ = true;
    const {success} =
        await this.traceReportProxy_.handler.userUploadSingleTrace(
            this.trace.uuid);
    if (!success) {
      this.dispatchToast_(`Failed to upload trace ${this.getTokenAsString_()}.`);
    } else {
      this.dispatchReloadRequest_();
    }
    this.isLoading_ = false;
  }

  protected uploadStateEqual_(state: ReportUploadState): boolean {
    return this.trace.uploadState === state;
  }

  protected getTokenAsString_(): string {
    return `${this.trace.uuid.high.toString(16)}-${
        this.trace.uuid.low.toString(16)}`;
  }

  private dispatchToast_(message: string): void {
    this.dispatchEvent(new CustomEvent('show-toast', {
      bubbles: true,
      composed: true,
      detail: new Notification(NotificationType.ERROR, message),
    }));
  }

  protected isDownloadDisabled_(): boolean {
    return this.isLoading_ || !this.trace.hasTraceContent;
  }

  protected getDownloadTooltip_(): string {
    return this.trace.hasTraceContent ? 'Download Trace' : 'Trace expired';
  }

  private dispatchReloadRequest_(): void {
    this.fire('refresh-traces-request');
  }

  protected getStateCssClass_(): string {
    switch (this.trace.uploadState) {
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

  protected getStateText_(): string {
    switch (this.trace.uploadState) {
      case ReportUploadState.kNotUploaded:
        return `Skip reason: ${this.getSkipReason_()}`;
      case ReportUploadState.kPending:
        return 'Pending upload';
      case ReportUploadState.kPending_UserRequested:
        return 'Pending upload: User requested';
      case ReportUploadState.kUploaded:
        return `Uploaded: ${this.dateToString_(this.trace.uploadTime)}`;
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

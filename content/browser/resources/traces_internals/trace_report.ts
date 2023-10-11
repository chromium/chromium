// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {assert} from 'chrome://resources/js/assert.js';
import {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './trace_report.html.js';
import {ClientTraceReport} from './trace_report.mojom-webui.js';
import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';

enum UploadState {
  NOT_UPLOADED = 0,
  PENDING = 1,
  USER_REQUEST = 2,
  UPLOADED = 3,
}

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

export class TraceReportElement extends PolymerElement {
  static get is() {
    return 'trace-report';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      trace: Object,
      // Enable the html template to use UploadState
      uploadStateEnum_: {
        type: Object,
        value: UploadState,
        readOnly: true,
      },
      isLoading: Boolean,
    };
  }

  private trace: ClientTraceReport;
  private isLoading: boolean = false;
  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();

  private onCopyUuidClick_(): void {
    // Get the text field
    assert(this.trace.uuid.high);
    assert(this.trace.uuid.low);
    navigator.clipboard.writeText(
        `${this.trace.uuid.high}-${this.trace.uuid.low}`);
  }

  private onCopyScenarioClick_(): void {
    // Get the text field
    assert(this.trace.scenarioName);
    navigator.clipboard.writeText(this.trace.scenarioName);
  }

  private onCopyUploadRuleClick_(): void {
    // Get the text field
    assert(this.trace.uploadRuleName);
    navigator.clipboard.writeText(this.trace.uploadRuleName);
  }

  private getSkipReason_(skipReason: number): string {
    // Keep this in sync with the values of SkipUploadReason in
    // trace_report.mojom
    const skipReasonMap: string[] = [
      'None',
      'Size limit exceeded',
      'Not anonymized',
      'Scenario quota exceeded',
    ];

    return skipReasonMap[skipReason];
  }

  private getTraceSize_(size: bigint): string {
    if (this.trace.totalSize < 1) {
      return '0 Bytes';
    }

    let displayedSize = Number(size);
    const k = 1024;

    const sizes = ['Bytes', 'KB', 'MB', 'GB'];

    let i = 0;

    for (i; displayedSize >= k && i < 3; i++) {
      displayedSize /= k;
    }

    return `${displayedSize.toFixed(2)} ${sizes[i]}`;
  }

  private dateToString_(mojoTime: Time): string {
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

  private async onDownloadTraceClick_(): Promise<void> {
    this.isLoading = true;
    const {trace} =
        await this.traceReportProxy_.handler.downloadTrace(this.trace.uuid);
    if (trace !== null) {
      // TODO(b/299476756): |result| can be empty/null/false in some
      // methods which should be handled differently than currently
      // for the user to know if an action has return the value
      // expected or not. Not simply if the call to the method failed
      this.downloadData_(
          `${this.trace.uuid.high}-${this.trace.uuid.low}.gz`, trace);
    }
    this.isLoading = false;
  }

  private downloadData_(fileName: string, data: BigBuffer): void {
    if (data.invalidBuffer) {
      console.error('Invalid buffer received');
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
      console.error('Unable to create blob from trace data', e);
    }
  }

  private async onDeleteTraceClick_(): Promise<void> {
    this.isLoading = true;
    const {success} =
        await this.traceReportProxy_.handler.deleteSingleTrace(this.trace.uuid);
    if (success) {
      // TODO(b/299476756): |result| can be empty/null/false in some
      // methods which should be handled differently than currently
      // for the user to know if an action has return the value
      // expected or not. Not simply if the call to the method failed
    }
    this.isLoading = false;
  }

  private async onUploadTraceClick_(): Promise<void> {
    this.isLoading = true;
    const {success} =
        await this.traceReportProxy_.handler.userUploadSingleTrace(
            this.trace.uuid);
    if (success) {
      // TODO(b/299476756): |result| can be empty/null/false in some
      // methods which should be handled differently than currently
      // for the user to know if an action has return the value
      // expected or not. Not simply if the call to the method failed
    }
    this.isLoading = false;
  }

  private uploadStateEqual(value1: number, value2: UploadState): boolean {
    return value1 === value2;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report': TraceReportElement;
  }
}

customElements.define(TraceReportElement.is, TraceReportElement);

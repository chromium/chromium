// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './trace_report.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ClientTraceReport} from './trace_report.mojom-webui.js';
import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {getTemplate} from './trace_report_list.html.js';

export class TraceReportListElement extends PolymerElement {
  static get is() {
    return 'trace-report-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      traces: Array,
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();
  traces: ClientTraceReport[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.initializeList();
  }

  private async initializeList(): Promise<void> {
    // TODO(b/299476756): |result| can be empty/null/false in some methods
    // which should be handled differently than currently for the user to
    // know if an action has return the value expected or not. Not simply
    // if the call to the method failed.
    const {reports} = await this.traceReportProxy_.handler.getAllTraceReports();
    this.traces = reports;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-report-list': TraceReportListElement;
  }
}

customElements.define(TraceReportListElement.is, TraceReportListElement);

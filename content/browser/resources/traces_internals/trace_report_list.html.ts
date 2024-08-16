// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ClientTraceReport} from './trace_report.mojom-webui.js';
import type {TraceReportListElement} from './trace_report_list.js';

function getReportHtml(this: TraceReportListElement) {
  // clang-format off
  if (!this.hasTraces_()) {
    return html`
    <div class="empty-message">
      <cr-icon icon="cr:warning"></cr-icon>
      <h1>Could not find any traces saved locally.</h1>
    </div>`;
  }

  return html`${this.traces_.map((traceReport: ClientTraceReport) => html`
    <trace-report .trace="${traceReport}"></trace-report>`)}`;
  // clang-format on
}

export function getHtml(this: TraceReportListElement) {
  // clang-format off
  return html`
  <div class="header">
    <h1>Traces
      <span class="trace-counter" ?hidden="${!this.hasTraces_()}">
        ${this.traces_.length}
      </span>
    </h1>
    ${this.hasTraces_() ? html`
    <div class="utility-bar">
      <cr-button class="tonal-button" ?disabled="${!this.hasTraces_()}"
          @click="${this.onDeleteAllTracesClick_}">
        <cr-icon icon="cr:delete" slot="prefix-icon"></cr-icon>
        Delete All Traces
      </cr-button>
    </div>` : nothing}
  </div>
  ${this.isLoading_ ? html`
  <div class="loading-spinner"><div class="spinner"></div></div>` :
  html`
  <div class="report-list-container">
    ${getReportHtml.bind(this)()}
  </div>`}
  <cr-toast id="toast" duration="5000" ?hidden="${!this.notification_}">
    <div id="notification-card">
      <div class="icon-container ${this.getNotificationStyling_()}">
        <cr-icon icon="${this.getNotificationIcon_()}"></cr-icon>
      </div>
      <div class="notification-message">
        <h4 class="notification-type ${this.getNotificationStyling_()}">
          ${this.getNotificationType_()}
        </h4>
        <span class="notification-label">${this.getNotificationLabel_()}</span>
      </div>
    </div>
  </cr-toast>`;
  // clang-format on
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TraceReportListElement} from './trace_report_list.js';

export function getHtml(this: TraceReportListElement) {
  // clang-format off
  return html`
    <div class="traces-header">
      <h1>Traces
        <span class="trace-counter" ?hidden="${!this.hasTraces_()}">
          ${this.traces.length}
        </span>
      </h1>
    </div>
    ${this.isLoading ? html`
      <div class="loading-spinner">
        <paper-spinner-lite ?active="${this.isLoading}">
        </paper-spinner-lite>
      </div>` : ''}

    ${!this.isLoading && this.hasTraces_() ? html`
      <div class="utility-bar">
        <cr-button class="floating-button"
            ?disabled="${!this.hasTraces_()}"
            @click="${this.onDeleteAllTracesClick_}">
          <iron-icon icon="cr:delete" aria-hidden="true"></iron-icon>
          Delete All Traces
        </cr-button>
      </div>
      ${this.traces.map(item => html`
        <trace-report .trace="${item}" @show-toast="${this.showToastHandler_}"
            @refresh-traces-request="${this.initializeList}">
        </trace-report>
        `)}` : html`
      <div class="empty-message" ?hidden="${this.hasTraces_()}">
        <iron-icon icon="cr:warning"></iron-icon>
        <h1>Could not find any traces saved locally.</h1>
      </div>`}

    <cr-toast id="toast" duration="5000" ?hidden="${!this.notification}">
      <div id="notification-card">
        <div class="icon-container ${this.getNotificationStyling_()}">
          <iron-icon icon="${this.getNotificationIcon_()}" aria-hidden="true">
          </iron-icon>
        </div>
        <div class="notification-message">
          <h4 class="notification-type ${this.getNotificationStyling_()}">
            ${this.notification.type}
          </h4>
          <span class="notification-label">${this.notification.label}</span>
        </div>
      </div>
    </cr-toast>`;
  // clang-format on
}

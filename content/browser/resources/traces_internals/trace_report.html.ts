// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {TraceReportElement} from './trace_report.js';

export function getHtml(this: TraceReportElement) {
  // clang-format off
  return html`
    <div class="trace-card">
      ${this.isLoading ? html`
        <div class="trace-spinner">
          <paper-spinner-lite active></paper-spinner-lite>
        </div>` : ''}
      <div class="trace-id-container">
        <button class="clickable-field copiable"
            @click="${this.onCopyUuidClick_}"
            title="${this.tokenToString_(this.trace.uuid)}">
          ${this.tokenToString_(this.trace.uuid)}
        </button>
        <div class="info">Trace ID</div>
      </div>
      <div class="trace-date-created-container">
        <div class="date-creation-value">
          ${this.dateToString_(this.trace.creationTime)}
        </div>
        <div class="info">Date created</div>
      </div>
      <div class="trace-scenario-container">
        <button class="clickable-field copiable"
            @click="${this.onCopyScenarioClick_}"
            title="${this.trace.scenarioName}">
          ${this.trace.scenarioName}
        </button>
      </div>
      <div class="trace-trigger-container">
        <button class="clickable-field copiable"
            @click="${this.onCopyUploadRuleClick_}"
            title="${this.trace.uploadRuleName}">
          ${this.trace.uploadRuleName}
        </button>
        <div class="info">Trigger rule</div>
      </div>
      <div class="trace-size-container">
        <div class="trace-size-value">${this.getTraceSize_()}</div>
        <div class="info">Uncompressed size</div>
      </div>
      <div class="trace-upload-state-container">
        <div class="upload-state-card state-default"
            ?hidden="${!this.uploadStateEqual_(
                this.uploadStateEnum_.NOT_UPLOADED)}"
            title="Skip reason: ${this.getSkipReason_()}">
          Skip reason: ${this.getSkipReason_()}
        </div>
        <div class="upload-state-card state-pending"
            ?hidden="${!this.uploadStateEqual_(this.uploadStateEnum_.PENDING)}">
          Pending upload
        </div>
        <div class="upload-state-card state-pending"
            ?hidden="${!this.uploadStateEqual_(
                this.uploadStateEnum_.USER_REQUEST)}">
          Pending upload: User requested
        </div>
        <div class="upload-state-card state-success"
            ?hidden="${!this.uploadStateEqual_(
                this.uploadStateEnum_.UPLOADED)}">
          Uploaded: ${this.dateToString_(this.trace.uploadTime)}
        </div>
      </div>
      <div class="actions-container">
        <cr-icon-button class="action-button" title="Upload Trace"
            iron-icon="trace-report-icons:cloud_upload"
            aria-label="Upload Trace" @click="${this.onUploadTraceClick_}"
            ?hidden="${!this.uploadStateEqual_(
                this.uploadStateEnum_.NOT_UPLOADED)}"
            ?disabled="${!this.isManualUploadPermitted_(
                this.trace.skipReason)}">
        </cr-icon-button>
        <cr-icon-button class="action-button" iron-icon="cr:file-download"
            title="${this.getDownloadTooltip_()}"
            @click="${this.onDownloadTraceClick_}"
            ?disabled="${this.isDownloadDisabled_()}"
            aria-label="${this.getDownloadTooltip_()}">
        </cr-icon-button>
        <cr-icon-button class="action-button" iron-icon="cr:delete"
            title="Delete Trace" @click="${this.onDeleteTraceClick_}"
            ?disabled="${this.isLoading}"
            aria-label="Delete Trace">
        </cr-icon-button>
      </div>
    </div>`;
  // clang-format on
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TraceReportElement} from './trace_report.js';
import {ReportUploadState} from './trace_report.mojom-webui.js';

export function getHtml(this: TraceReportElement) {
  // clang-format off
  return html`
    ${this.isLoading_ ? html`<div class="spinner"></div>` :
    html`
    <div class="trace-id-container">
      <button class="clickable-field copiable" title="${this.getTokenAsString_()}"
          @click="${this.onCopyUuidClick_}">
        ${this.getTokenAsString_()}
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
      <button class="clickable-field copiable" title="${this.trace.scenarioName}"
          @click="${this.onCopyScenarioClick_}">
        ${this.trace.scenarioName}
      </button>
    </div>
    <div class="trace-trigger-container">
      <button class="clickable-field copiable" title="${this.trace.uploadRuleName}"
          @click="${this.onCopyUploadRuleClick_}">
        ${this.trace.uploadRuleName}
      </button>
      <div class="info">Trigger rule</div>
    </div>
    <div class="trace-size-container">
      <div class="trace-size-value">${this.getTraceSize_()}</div>
      <div class="info">Uncompressed size</div>
    </div>
    <div class="trace-upload-state-container">
      <div class="upload-state-card ${this.getStateCssClass_()}"
        title="${this.getStateText_()}">
        ${this.getStateText_()}
      </div>
    </div>
    <div class="actions-container">
      <cr-icon-button class="action-button" title="Upload Trace"
          iron-icon="trace-report-icons:cloud_upload"
          ?hidden="${!this.uploadStateEqual_(ReportUploadState.kNotUploaded)}"
          ?disabled="${!this.isManualUploadPermitted_()}"
          @click="${this.onUploadTraceClick_}">
      </cr-icon-button>
      <cr-icon-button class="action-button download"
          iron-icon="cr:file-download" title="${this.getDownloadTooltip_()}"
          @click="${this.onDownloadTraceClick_}"
          ?disabled="${this.isDownloadDisabled_()}">
      </cr-icon-button>
      <cr-icon-button class="action-button" iron-icon="cr:delete"
          title="Delete Trace" @click="${this.onDeleteTraceClick_}"
          ?disabled="${this.isLoading_}">
      </cr-icon-button>
    </div>
  `}`;
  // clang-format on
}

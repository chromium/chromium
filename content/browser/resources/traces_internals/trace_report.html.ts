// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {TraceReportElement} from './trace_report.js';
import {getTokenAsUuidString} from './trace_util.js';
import {ReportUploadState} from './traces_internals.mojom-webui.js';

export function getHtml(this: TraceReportElement) {
  // clang-format off
  return this.isHeader ? html`
    <div class="info">Trace ID</div>
    <div class="info">Date created</div>
    <div class="info">Scenario</div>
    <div class="info">Triggered rule</div>
    <div class="info">Uncompressed size</div>` : (this.trace !== null ?
    html`<div>
      <button class="clickable-field copiable"
          title="${getTokenAsUuidString(this.trace.uuid)}"
          @click="${this.onCopyUuidClick_}">
        ${getTokenAsUuidString(this.trace.uuid)}
      </button>
    </div>
    <div class="value">
      ${this.dateToString_(this.trace.creationTime)}
    </div>
    <div>
      <button class="clickable-field copiable"
          title="${this.trace.scenarioName}"
          @click="${this.onCopyScenarioClick_}">
        ${this.trace.scenarioName}
      </button>
    </div>
    <div>
      <button class="clickable-field copiable" title="${this.trace.uploadRuleName}"
          @click="${this.onCopyUploadRuleClick_}">
        ${this.trace.uploadRuleName}
      </button>
      ${this.trace.uploadRuleValue !== null ? html`
        <div class="value">
          Value: ${this.trace.uploadRuleValue}
        </div>
      ` : nothing}
    </div>
    <div class="value">${this.getTraceSize_(this.trace)}</div>
    <div class="upload-state-card ${this.getStateCssClass_(this.trace)}"
      title="${this.getStateText_(this.trace)}">
      ${this.getStateText_(this.trace)}
    </div>
    <div class="actions-container">
    <cr-icon-button class="action-button" title="Upload Trace"
          iron-icon="trace-report-icons:cloud_upload"
          ?hidden="${!this.uploadStateEqual_(
            this.trace, ReportUploadState.kNotUploaded)}"
          ?disabled="${this.isManualUploadDisabled_(this.trace)}"
          @click="${this.onUploadTraceClick_}">
      </cr-icon-button>
      <cr-icon-button class="action-button download"
          iron-icon="cr:file-download" title="${
            this.getDownloadTooltip_(this.trace)}"
          @click="${this.onDownloadTraceClick_}"
          ?disabled="${this.isDownloadDisabled_(this.trace)}">
      </cr-icon-button>
      <cr-icon-button class="action-button" iron-icon="cr:delete"
          title="Delete Trace" @click="${this.onDeleteTraceClick_}"
          ?disabled="${this.isLoading}">
      </cr-icon-button>
    </div>
    ` : nothing);
  // clang-format on
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {TraceRecorderElement} from './trace_recorder.js';

export function getHtml(this: TraceRecorderElement) {
  // clang-format off
  return html`
    <h1>Record Trace</h1>
    <div id="config-container">
      Your current config proto: ${this.traceConfig}
    </div>
    <div id="status-container" class="${this.statusClass}">
        <div class="status-circle"></div>
        <h3>Status: ${this.tracingState}</h3>
    </div>
    <div id="action-panel">
      <cr-button
          @click="${this.startTracing_}"
          ?disabled="${!this.isStartTracingEnabled}">
        Start Tracing
      </cr-button>
      <cr-button
          @click="${this.stopTracing_}"
          ?disabled="${!this.isStopTracingEnabled}">
        Stop Tracing
      </cr-button>
      <cr-button
          @click="${this.cloneTraceSession_}"
          ?disabled="${!this.isCloneTraceEnabled}">
        Snapshot Trace
      </cr-button>
    </div>
    <cr-toast id="toast" duration="5000">
      <div>${this.toastMessage}</div>
    </cr-toast>
  `;
  // clang-format on
}

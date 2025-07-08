// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {TraceRecorderElement} from './trace_recorder.js';

export function getHtml(this: TraceRecorderElement) {
  // clang-format off
  return html`
    <h1>Record Trace</h1>
    <div id="control-container">
      <div id="status-container" class="${this.statusClass}">
        <div class="status-circle"></div>
        <h3>Status: ${this.tracingState}</h3>
      </div>
      <div id="progress-container"
          ?hidden="${!this.isRecording}">
        <label for="buffer-progress">Buffer Usage:
            ${Math.round(this.bufferUsage * 100)}%
        </label>
        <progress id="buffer-progress" max="100"
            .value="${this.bufferUsage * 100}">
        </progress>
      </div>
      <div id="action-panel">
        <cr-button
            @click="${this.startTracing_}"
            ?disabled="${!this.isStartTracingEnabled}">
          Start Tracing
        </cr-button>
        <cr-button
            @click="${this.stopTracing_}"
            ?disabled="${!this.isRecording}">
          Stop Tracing
        </cr-button>
        <cr-button
            @click="${this.cloneTraceSession_}"
            ?disabled="${!this.isRecording}">
          Snapshot Trace
        </cr-button>
      </div>
    </div>

    <div class="card">
      <cr-expand-button class="cr-row" ?expanded="${this.categoriesExpanded_}"
          @expanded-changed="${this.onCategoriesExpandedChanged_}">
        Track Event Categories
      </cr-expand-button>
      <cr-collapse id="expanded-content" ?opened="${this.categoriesExpanded_}">
        <p>Select the categories to include in the trace config.</p>
        <div class="category-grid">
          <div class="header-row-group">
            <div>Enabled</div>
            <div>Name</div>
            <div>Tags</div>
            <div>Description</div>
          </div>
          ${this.traceCategories.map(category => html`
            <div class="category-row">
              <input
                type="checkbox"
                .checked="${this.isEnabled(category.name)}"
                @change="${
                  (e: Event) => this.onCategoryChange_(e, category.name)}">
              <div>${category.name}</div>
              <div>${category.tags.join(', ')}</div>
              <div>${category.description}</div>
            </div>
          `)}
        </div>
      </cr-collapse>
    </div>

    <cr-toast id="toast" duration="5000">
      <div>${this.toastMessage}</div>
    </cr-toast>
  `;
  // clang-format on
}

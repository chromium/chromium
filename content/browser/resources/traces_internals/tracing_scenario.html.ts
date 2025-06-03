// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TracingScenarioElement} from './tracing_scenario.js';

export function getHtml(this: TracingScenarioElement) {
  // clang-format off
  // Field scenario checkboxes are always disabled because the can't be modified
  // individually.
  return html`
  <cr-toggle
      ?checked="${this.enabled}"
      ?disabled="${!this.scenario.isLocalScenario}"
      @change="${this.onEnabledChange_}">
  </cr-toggle>
  <div class="current-state-card ${this.getCurrentStateCssClass_()}">
    ${this.getCurrentStateText_()}
  </div>
  <div class="info">${this.scenario.scenarioName}</div>
  <div class="info">${this.scenario.description}</div>
  `;
  // clang-format on
}

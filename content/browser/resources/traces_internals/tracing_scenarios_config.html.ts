// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {TracingScenariosConfigElement} from './tracing_scenarios_config.js';

function getPresetConfigHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  if (this.presetConfig_ === null || this.presetConfig_.length <= 0) {
    return nothing;
  }

  return html`
    <h2>Local Scenarios</h2>
    <div class="config-container">
    ${this.presetConfig_.map((item, index) => html`
      <div class="config-row">
        <cr-checkbox ?checked="${item.selected}" data-index="${index}"
            @checked-changed="${this.valueDidChange_}">
          <div class="label">${item.scenarioName}</div>
        </cr-checkbox>
      </div>`)}
    </div>`;
  // clang-format on
}

function getFieldConfigHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  if (this.fieldConfig_ === null || this.fieldConfig_.length <= 0) {
    return nothing;
  }

  // Field scenario checkboxes are always disabled because the can't be modified
  // individually.
  return html`
    <h2>Field Scenarios</h2>
    <div class="config-container">
    ${this.fieldConfig_.map((item, index) => html`
      <div class="config-row">
        <cr-checkbox ?checked="${item.selected}" data-index="${index}" disabled>
          <div class="label">${item.scenarioName}</div>
        </cr-checkbox>
      </div>`)}
    </div>`;
  // clang-format on
}

export function getHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  return html`
  <h1>Tracing configuration</h1>
  <h3>
    This configuration is designed for local trace collection, enabling you to
    capture detailed information about application execution on your machine.
  </h3>
  ${this.isLoading_ ? html`<div class="spinner"></div>` : html`
  ${getFieldConfigHtml.bind(this)()}
  ${getPresetConfigHtml.bind(this)()}
  <div class="action-panel">
    <cr-button class="cancel-button" ?disabled="${!this.isEdited_}"
        @click="${this.onCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button" ?disabled="${!this.hasSelectedConfig_()}"
        @click="${this.clearAllClick_}">
      Clear
    </cr-button>
    <cr-button class="action-button" ?disabled="${!this.isEdited_}"
        @click="${this.onConfirmClick_}">
      Confirm
    </cr-button>
  </div>`
  }
  <cr-toast id="toast" duration="5000">${this.toastMessage_}</cr-toast>
  `;
  // clang-format on
}

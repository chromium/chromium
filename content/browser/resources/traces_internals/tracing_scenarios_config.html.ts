// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {TracingScenariosConfigElement} from './tracing_scenarios_config.js';

function getPresetConfigHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  if (this.localConfig_ === null || this.localConfig_.length <= 0) {
    return nothing;
  }

  return html`
  <h2>Local Scenarios</h2>
  <div class="scenario-list-container">
  ${this.localConfig_.map((item) => html`
    <tracing-scenario
        .scenario="${item}"
        .enabled="${this.isScenarioEnabled_(item)}"
        @value-changed="${this.valueDidChange_}"
        data-key="${item.scenarioName}">
    </tracing-scenario>`)}
  </div>`;
  // clang-format on
}

function getFieldConfigHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  if (this.fieldConfig_ === null || this.fieldConfig_.length <= 0) {
    return nothing;
  }

  return html`
  <h2>Field Scenarios</h2>
  <div class="scenario-list-container">
  ${this.fieldConfig_.map((item, index) => html`
    <tracing-scenario
        .scenario="${item}"
        .enabled="${item.isEnabled}"
        data-index="${index}">
    </tracing-scenario>
  `)}
  </div>`;
  // clang-format on
}

function getSystemTracingHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  // <if expr="is_win">
  if (!this.tracingServiceSupported_) {
    return nothing;
  }

  return html`
    <h2>System Tracing</h2>
    <div class="config-toggle-container">
      <div class="config-toggle-description">
        <em>Enable system tracing</em>
        <span>When on, traces include system-wide events. You must have
          administrative rights on your computer to modify this setting.</span>
      </div>
      <cr-toggle
          class="config-toggle"
          ?checked="${this.tracingServiceRegistered_}"
          @change="${this.onSystemTracingChange_}">
      </cr-toggle>
      ${this.securityShieldIconUrl_
          ? html`<img id="system-tracing-shield"
                      src="${this.securityShieldIconUrl_}"/>`
          : nothing}
    </div>`;
  // </if>
  // <if expr="not is_win">
  return nothing;
  // </if>
  // clang-format on
}

function getPrivacyFilterHtml(this: TracingScenariosConfigElement) {
  return html`
    <h2>Privacy Filters</h2>
    <div class="config-toggle-container">
      <div class="config-toggle-description">
        <em>Enable privacy filters</em>
        <span>Remove untyped and sensitive data like URLs from local scenarios.
        Needs restart to take effect.</span>
      </div>
      <cr-toggle
          class="config-toggle"
          ?checked="${this.privacyFilterEnabled_}"
          @change="${this.privacyFilterDidChange_}">
      </cr-toggle>
    </div>`;
}

export function getHtml(this: TracingScenariosConfigElement) {
  // clang-format off
  return html`
  <h1>Tracing configuration</h1>
  <h3>
    This configuration is designed for local trace collection, enabling you to
    capture detailed information about application execution on your machine.
  </h3>
  ${getPrivacyFilterHtml.bind(this)()}
  ${getSystemTracingHtml.bind(this)()}
  <h2>Scenarios Config</h2>
  <h3>
    You can select a proto (.pb) or base64 encoded (.txt) file that contains
    scenarios config. For details, see
    <a href="http://go/how-do-i-chrometto#how-do-i-test-background-tracing-setup-locally.">
      how-do-i-chrometto
    </a>
  </h3>
  <input type="file" class="action-button" name="Choose File"
      @change="${this.onAddConfig_}">
  </input>
  ${this.isLoading_ ? html`<div class="spinner"></div>` : html`
  ${getFieldConfigHtml.bind(this)()}
  ${getPresetConfigHtml.bind(this)()}
  <div class="action-panel">
    <cr-button class="action-button"
        @click="${this.resetAllClick_}">
      Reset
    </cr-button>
    <cr-button class="cancel-button"
        ?disabled="${Object.keys(this.enabledScenarios_).length === 0}"
        @click="${this.onCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button"
        ?disabled="${Object.keys(this.enabledScenarios_).length === 0}"
        @click="${this.onConfirmClick_}">
      Confirm
    </cr-button>
  </div>`
  }
  <cr-toast id="toast" duration="5000">
    <div>${this.toastMessage_}</div>
  </cr-toast>
  `;
  // clang-format on
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './tracing_scenario.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
// <if expr="is_win">
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
// </if>
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {TracesBrowserProxy} from './traces_browser_proxy.js';
import type {Scenario} from './traces_internals.mojom-webui.js';
import {getCss} from './tracing_scenarios_config.css.js';
import {getHtml} from './tracing_scenarios_config.html.js';

export interface TracingScenariosConfigElement {
  $: {
    toast: CrToastElement,
  };
}

export class TracingScenariosConfigElement extends CrLitElement {
  static get is() {
    return 'tracing-scenarios-config';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enabledScenarios_: {type: Object},
      localConfig_: {type: Array},
      fieldConfig_: {type: Array},
      isLoading_: {type: Boolean},
      privacyFilterEnabled_: {type: Boolean},
      toastMessage_: {type: String},
      // <if expr="is_win">
      tracingServiceSupported_: {type: Boolean},
      tracingServiceRegistered_: {type: Boolean},
      // </if>
    };
  }

  private traceReportProxy_: TracesBrowserProxy =
      TracesBrowserProxy.getInstance();

  private refreshIntervalId_: number = 0;

  protected accessor enabledScenarios_: {[id: string]: boolean} = {};
  protected accessor localConfig_: Scenario[] = [];
  protected accessor fieldConfig_: Scenario[] = [];
  protected accessor isLoading_: boolean = false;
  protected accessor privacyFilterEnabled_: boolean = false;
  protected accessor toastMessage_: string = '';
  // <if expr="is_win">
  protected accessor tracingServiceSupported_: boolean = false;
  protected accessor tracingServiceRegistered_: boolean = false;
  protected securityShieldIconUrl_: string = '';
  // </if>

  override connectedCallback(): void {
    super.connectedCallback();
    this.initializeConfig_();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    clearInterval(this.refreshIntervalId_);
  }

  protected async initializeConfig_(): Promise<void> {
    this.isLoading_ = true;

    await this.loadScenariosConfig_();

    this.privacyFilterEnabled_ =
        (await this.traceReportProxy_.handler.getPrivacyFilterEnabled())
            .enabled;

    // <if expr="is_win">
    const {
      serviceSupported: serviceSupported,
      serviceRegistered: serviceRegistered,
    } = await this.traceReportProxy_.handler.getSystemTracingState();

    this.tracingServiceSupported_ = serviceSupported;
    this.tracingServiceRegistered_ = serviceRegistered;

    if (this.tracingServiceSupported_) {
      this.securityShieldIconUrl_ =
          (await this.traceReportProxy_.handler.getSecurityShieldIconUrl())
              .shieldIconUrl.url;
    }
    // </if>

    this.isLoading_ = false;

    this.refreshIntervalId_ =
        setInterval(this.loadScenariosConfig_.bind(this), 1000);
  }

  protected async loadScenariosConfig_(): Promise<void> {
    const {config: scenarios} =
        await this.traceReportProxy_.handler.getAllScenarios();
    this.localConfig_ = [];
    this.fieldConfig_ = [];
    for (const scenario of scenarios) {
      if (scenario.isLocalScenario) {
        this.localConfig_.push(scenario);
      } else {
        this.fieldConfig_.push(scenario);
      }
    }
  }

  protected isScenarioEnabled_(scenario: Scenario) {
    return this.enabledScenarios_[scenario.scenarioName] ?? scenario.isEnabled;
  }

  protected async privacyFilterDidChange_(event: CustomEvent<boolean>):
      Promise<void> {
    if (this.privacyFilterEnabled_ === event.detail) {
      return;
    }
    this.privacyFilterEnabled_ = event.detail;
    await this.traceReportProxy_.handler.setPrivacyFilterEnabled(
        this.privacyFilterEnabled_);
  }

  protected valueDidChange_(event: CustomEvent<{value: boolean}>): void {
    const key = (event.currentTarget as HTMLElement).dataset['key'];
    if (key === undefined) {
      return;
    }
    this.enabledScenarios_[key] = event.detail.value;
  }

  protected onCancelClick_() {
    this.enabledScenarios_ = {};
  }

  protected async onConfirmClick_(): Promise<void> {
    const enabledScenarios: string[] = [];
    for (const scenario of this.localConfig_) {
      if (this.enabledScenarios_[scenario.scenarioName] ?? scenario.isEnabled) {
        enabledScenarios.push(scenario.scenarioName);
      }
    }
    const {success} = await this.traceReportProxy_.handler.setEnabledScenarios(
        enabledScenarios);
    if (!success) {
      this.toastMessage_ = 'Failed to update scenarios config';
      this.$.toast.show();
      return;
    }
    await this.loadScenariosConfig_();
    this.enabledScenarios_ = {};
  }

  protected async onAddConfig_(e: Event&
                               {target: HTMLInputElement}): Promise<void> {
    const files = e.target.files;
    if (!files) {
      this.toastMessage_ = `Failed to open config file.`;
      this.$.toast.show();
      return;
    }

    for (const file of files) {
      const result = await this.processConfigFile_(file);
      if (!result.success) {
        this.toastMessage_ = `Failed to read config file ${file.name}.`;
        this.$.toast.show();
      }
    }

    await this.loadScenariosConfig_();
  }

  private async processConfigFile_(file: File): Promise<{success: boolean}> {
    const isTextFile = (file.type === 'text/plain');
    const handler = this.traceReportProxy_.handler;

    if (isTextFile) {
      const text = await file.text();
      return handler.setScenariosConfigFromString(text);
    } else {
      const bytes = await file.arrayBuffer();
      const buffer: BigBuffer = {bytes: Array.from(new Uint8Array(bytes))} as
          any;
      return handler.setScenariosConfigFromBuffer(buffer);
    }
  }

  protected async resetAllClick_(): Promise<void> {
    const {success} =
        await this.traceReportProxy_.handler.setEnabledScenarios([]);
    if (!success) {
      this.toastMessage_ = 'Failed to reset scenarios';
      this.$.toast.show();
      return;
    }
    await this.loadScenariosConfig_();
    this.enabledScenarios_ = {};
  }

  // <if expr="is_win">
  protected async onSystemTracingChange_(e: CustomEvent<boolean>) {
    const enable = e.detail;
    if (enable === this.tracingServiceRegistered_) {
      return;
    }
    const target = (e.target as CrToggleElement);
    const {success} = enable ?
        await this.traceReportProxy_.handler.enableSystemTracing() :
        await this.traceReportProxy_.handler.disableSystemTracing();
    if (success) {
      // On success, update the instance's registration state.
      this.tracingServiceRegistered_ = enable;
    } else if (target) {
      // On failure, put the toggle back to its previous state.
      target.checked = !enable;
    }
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'tracing-scenarios-config': TracingScenariosConfigElement;
  }
}

customElements.define(
    TracingScenariosConfigElement.is, TracingScenariosConfigElement);

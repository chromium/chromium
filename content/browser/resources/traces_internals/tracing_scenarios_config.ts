// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
// <if expr="is_win">
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
// </if>
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {getCss} from './tracing_scenarios_config.css.js';
import {getHtml} from './tracing_scenarios_config.html.js';

interface Config {
  scenarioName: string;
  selected: boolean;
}

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
      presetConfig_: {type: Array},
      fieldConfig_: {type: Array},
      isEdited_: {type: Boolean},
      isLoading_: {type: Boolean},
      privacyFilterEnabled_: {type: Boolean},
      toastMessage_: {type: String},
      // <if expr="is_win">
      tracingServiceSupported_: {type: Boolean},
      tracingServiceRegistered_: {type: Boolean},
      // </if>
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();

  protected accessor presetConfig_: Config[] = [];
  protected accessor fieldConfig_: Config[] = [];
  protected accessor isEdited_: boolean = false;
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
    this.initScenariosConfig_();
  }

  private async initScenariosConfig_(): Promise<void> {
    this.isLoading_ = true;
    this.isEdited_ = false;
    this.presetConfig_ = [];
    this.fieldConfig_ = [];
    this.privacyFilterEnabled_ =
        (await this.traceReportProxy_.handler.getPrivacyFilterEnabled())
            .enabled;

    const enabledList =
        await this.traceReportProxy_.handler.getEnabledScenarios();
    const enabledSet: Set<string> = new Set(enabledList.config);

    const {config: presetScenarios} =
        await this.traceReportProxy_.handler.getAllPresetScenarios();
    const {config: fieldScenarios} =
        await this.traceReportProxy_.handler.getAllFieldScenarios();

    for (const scenario of presetScenarios) {
      const isSelected = enabledSet.has(scenario.scenarioName);
      this.presetConfig_.push({
        scenarioName: scenario.scenarioName,
        selected: isSelected,
      });
    }
    for (const scenario of fieldScenarios) {
      const isSelected = enabledSet.has(scenario.scenarioName);
      this.fieldConfig_.push({
        scenarioName: scenario.scenarioName,
        selected: isSelected,
      });
    }

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
    const index = Number((event.currentTarget as HTMLElement).dataset['index']);
    if (this.presetConfig_[index] === undefined) {
      this.toastMessage_ = 'Failed to find selected scenario';
      this.$.toast.show();
      return;
    }

    if (this.presetConfig_[index].selected === event.detail.value) {
      return;
    }

    this.presetConfig_[index].selected = event.detail.value;
    this.isEdited_ = true;
  }

  protected async onConfirmClick_(): Promise<void> {
    const enabledScenarios: string[] = [];
    for (const scenario of this.presetConfig_) {
      if (scenario.selected) {
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
    await this.initScenariosConfig_();
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

    await this.initScenariosConfig_();
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

  protected async onCancelClick_(): Promise<void> {
    await this.initScenariosConfig_();
  }

  protected hasSelectedConfig_(): boolean {
    return this.presetConfig_.some(scenario => scenario.selected) ||
        this.fieldConfig_.some(scenario => scenario.selected);
  }

  protected async resetAllClick_(): Promise<void> {
    const {success} =
        await this.traceReportProxy_.handler.setEnabledScenarios([]);
    if (!success) {
      this.toastMessage_ = 'Failed to reset scenarios';
      this.$.toast.show();
      return;
    }
    await this.initScenariosConfig_();
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

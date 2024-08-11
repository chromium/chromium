// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TraceReportBrowserProxy} from './trace_report_browser_proxy.js';
import {getCss} from './tracing_scenarios_config.css.js';
import {getHtml} from './tracing_scenarios_config.html.js';

interface Config {
  scenarioName: string;
  selected: boolean;
  hash: string;
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
      currentConfig_: {type: Array},
      newConfig_: {type: Array},
      isEdited_: {type: Boolean},
      isLoading_: {type: Boolean},
      toastMessage_: {type: String},
    };
  }

  private traceReportProxy_: TraceReportBrowserProxy =
      TraceReportBrowserProxy.getInstance();

  protected currentConfig_: Config[] = [];
  protected isEdited_: boolean = false;
  protected isLoading_: boolean = false;
  protected toastMessage_: string = '';

  override connectedCallback(): void {
    super.connectedCallback();
    this.initScenariosConfig_();
  }

  private async initScenariosConfig_(): Promise<void> {
    this.isLoading_ = true;
    this.isEdited_ = false;
    this.currentConfig_ = [];

    const enabledList =
        await this.traceReportProxy_.handler.getEnabledScenarios();
    const enabledSet: Set<string> = new Set(enabledList.config);

    const {config} =
        await this.traceReportProxy_.handler.getAllPresetScenarios();

    for (const scenario of config) {
      const isSelected = enabledSet.has(scenario.hash);
      this.currentConfig_.push({
        scenarioName: scenario.scenarioName,
        selected: isSelected,
        hash: scenario.hash,
      });
    }

    this.isLoading_ = false;
  }

  protected valueDidChange_(event: CustomEvent<{value: boolean}>): void {
    const index = Number((event.currentTarget as HTMLElement).dataset['index']);
    if (this.currentConfig_[index] === undefined) {
      this.toastMessage_ = 'Failed to find selected scenario';
      this.$.toast.show();
      return;
    }

    if (this.currentConfig_[index].selected === event.detail.value) {
      return;
    }

    this.currentConfig_[index].selected = event.detail.value;
    this.isEdited_ = true;
  }

  protected async onConfirmClick_(): Promise<void> {
    const enabledScenarios: string[] = [];
    for (const scenario of this.currentConfig_) {
      if (scenario.selected) {
        enabledScenarios.push(scenario.hash);
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

  protected async onCancelClick_(): Promise<void> {
    await this.initScenariosConfig_();
  }

  protected hasSelectedConfig_(): boolean {
    return this.currentConfig_.some(scenario => scenario.selected);
  }

  protected clearAllClick_() {
    this.currentConfig_.forEach(config => {
      config.selected = false;
    });
    this.isEdited_ = true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tracing-scenarios-config': TracingScenariosConfigElement;
  }
}

customElements.define(
    TracingScenariosConfigElement.is, TracingScenariosConfigElement);

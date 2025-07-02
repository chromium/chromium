// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from '//resources/js/assert.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {TraceConfig} from './perfetto_config.js';
import type {TrackEventConfig} from './perfetto_config.js';
import {getCss} from './trace_recorder.css.js';
import {getHtml} from './trace_recorder.html.js';
import {TracesBrowserProxy} from './traces_browser_proxy.js';
import type {TraceCategory} from './traces_internals.mojom-webui.js';

enum TracingState {
  IDLE = 'Idle',
  STARTING = 'Starting',
  RECORDING = 'Recording',
  STOPPING = 'Stopping',
}

export interface TraceRecorderElement {
  $: {
    toast: CrToastElement,
  };
}

export class TraceRecorderElement extends CrLitElement {
  static get is() {
    return 'trace-recorder';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      traceConfig: {type: String},
      toastMessage: {type: String},
      tracingState: {type: String},
      traceCategories: {type: Array},
      trackEventConfig: {type: Object},
      categoriesExpanded_: {type: Boolean},
      bufferUsage: {type: Number},
      hadDataLoss: {type: Boolean},
    };
  }

  private browserProxy_: TracesBrowserProxy = TracesBrowserProxy.getInstance();

  // Bound method for router events
  private boundLoadConfigFromUrl_ = this.loadConfigFromUrl_.bind(this);
  // Bound method for onTraceComplete listener
  private boundOnTraceComplete_ = this.onTraceComplete_.bind(this);
  // Bound method for buffer usage polling
  private readonly boundPollBufferUsage_ = this.pollBufferUsage_.bind(this);

  // Property to store the listener ID for onTraceComplete
  private onTraceCompleteListenerId_: number|null = null;
  // ID for the polling interval
  private bufferPollIntervalId_: number|null = null;
  private encodedConfigString: string = '';

  protected accessor traceConfig: Uint8Array = new Uint8Array();
  protected accessor toastMessage: string = '';
  // Initialize the tracing state to IDLE.
  protected accessor tracingState: TracingState = TracingState.IDLE;
  protected accessor traceCategories: TraceCategory[] = [];
  protected accessor trackEventConfig: TrackEventConfig|undefined;
  protected accessor categoriesExpanded_: boolean = false;
  protected accessor bufferUsage: number = 0;
  protected accessor hadDataLoss: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.loadConfigFromUrl_();
    this.loadTraceCategories_();
    CrRouter.getInstance().addEventListener(
        'cr-router-path-changed', this.boundLoadConfigFromUrl_);
    this.onTraceCompleteListenerId_ =
        this.browserProxy_.callbackRouter.onTraceComplete.addListener(
            this.boundOnTraceComplete_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    CrRouter.getInstance().removeEventListener(
        'cr-router-path-changed', this.boundLoadConfigFromUrl_);
    if (this.onTraceCompleteListenerId_ !== null) {
      this.browserProxy_.callbackRouter.removeListener(
          this.onTraceCompleteListenerId_);
      this.onTraceCompleteListenerId_ = null;
    }
  }

  protected get isStartTracingEnabled(): boolean {
    return this.tracingState === TracingState.IDLE && !!this.traceConfig;
  }

  protected get isRecording(): boolean {
    return this.tracingState === TracingState.RECORDING;
  }

  protected get statusClass(): string {
    switch (this.tracingState) {
      case TracingState.IDLE:
        return 'status-idle';
      case TracingState.STARTING:
        return 'status-starting';
      case TracingState.RECORDING:
        return 'status-tracing';
      case TracingState.STOPPING:
        return 'status-stopping';
      default:
        return '';
    }
  }

  private async pollBufferUsage_(): Promise<void> {
    const {success, percentFull, dataLoss} =
        await this.browserProxy_.handler.getBufferUsage();

    if (success) {
      this.bufferUsage = percentFull;
      this.hadDataLoss = dataLoss;
    }
  }

  protected async startTracing_(): Promise<void> {
    const bigBufferConfig = this.decodeBase64ToBigBuffer_();
    if (!bigBufferConfig) {
      return;
    }

    // Set state to RECORDING immediately to disable start button.
    this.tracingState = TracingState.STARTING;

    const {success} =
        await this.browserProxy_.handler.startTraceSession(bigBufferConfig);

    if (!success) {
      this.showToast_('Failed to start tracing.');
      // Revert to IDLE if starting failed.
      this.tracingState = TracingState.IDLE;
    } else {
      this.tracingState = TracingState.RECORDING;
      this.bufferPollIntervalId_ =
          window.setInterval(this.boundPollBufferUsage_, 1000);
    }
  }

  protected async stopTracing_(): Promise<void> {
    if (this.bufferPollIntervalId_ !== null) {
      window.clearInterval(this.bufferPollIntervalId_);
      this.bufferPollIntervalId_ = null;
    }

    // Set state to STOPPING to indicate an ongoing operation.
    this.tracingState = TracingState.STOPPING;

    const {success} = await this.browserProxy_.handler.stopTraceSession();

    if (!success) {
      this.showToast_('Failed to stop tracing.');
    }
  }

  protected async cloneTraceSession_(): Promise<void> {
    const {trace} = await this.browserProxy_.handler.cloneTraceSession();
    this.downloadData_(trace);
  }

  protected onCategoriesExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.categoriesExpanded_ = e.detail.value;
  }

  protected isEnabled(categoryName: string): boolean {
    if (!this.trackEventConfig?.enabledCategories) {
      return false;
    }
    return this.trackEventConfig.enabledCategories.includes(categoryName);
  }

  private getArrayFromBigBuffer(bigBuffer: BigBuffer): Uint8Array {
    if (Array.isArray(bigBuffer.bytes)) {
      return new Uint8Array(bigBuffer.bytes);
    }
    assert(!!bigBuffer.sharedMemory, 'sharedMemory must be defined here');
    const sharedMemory = bigBuffer.sharedMemory;
    const {buffer, result} =
        sharedMemory.bufferHandle.mapBuffer(0, sharedMemory.size);
    assert(result === Mojo.RESULT_OK, 'Could not map buffer');
    return new Uint8Array(buffer);
  }

  private downloadData_(traceData: BigBuffer|null): void {
    if (!traceData) {
      this.showToast_('Failed to download trace or no trace data.');
      return;
    }
    try {
      const traceArray = this.getArrayFromBigBuffer(traceData);
      const blob = new Blob([traceArray], {
        type: 'application/octet-stream',
      });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;

      const now = new Date();
      a.download = `${
          now.toLocaleString(
              /*locales=*/ undefined, {
                hour: 'numeric',
                minute: 'numeric',
                month: 'short',
                day: 'numeric',
                year: 'numeric',
                hour12: true,
              })}.gz`;
      a.click();
    } catch (e) {
      this.showToast_(`Error downloading trace: ${e}`);
    }
  }

  private async loadTraceCategories_(): Promise<void> {
    const {categories} =
        await this.browserProxy_.handler.getTrackEventCategories();
    const disabledPrefix = 'disabled-by-default-';
    this.traceCategories =
        categories.filter((category: TraceCategory) => !category.isGroup)
            .sort(
                (a, b) =>
                    a.name.replace(disabledPrefix, '')
                        .localeCompare(b.name.replace(disabledPrefix, '')));
  }

  // Decodes a Base64 string into a Uint8Array.
  private base64ToUint8Array_(base64String: string): Uint8Array {
    const binaryString = atob(base64String);
    const len = binaryString.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return bytes;
  }

  private decodeBase64ToBigBuffer_(): BigBuffer|undefined {
    let bigBuffer: BigBuffer|undefined = undefined;
    try {
      bigBuffer = {
        bytes: Array.from(this.traceConfig),
      } as BigBuffer;
      return bigBuffer;
    } catch (error) {
      this.showToast_(`Error decoding Base64: ${error}`);
    }
    return bigBuffer;
  }

  private onTraceComplete_(trace: BigBuffer|null): void {
    if (this.bufferPollIntervalId_ !== null) {
      window.clearInterval(this.bufferPollIntervalId_);
      this.bufferPollIntervalId_ = null;
    }
    this.bufferUsage = 0;
    this.hadDataLoss = false;

    this.downloadData_(trace);

    // Crucially, only set to IDLE here after the trace has been
    // processed/handled.
    this.tracingState = TracingState.IDLE;
  }

  private showToast_(message: string): void {
    this.toastMessage = message;
    this.$.toast?.show();
  }

  private loadConfigFromUrl_(): void {
    const params = new URLSearchParams(document.location.search);
    const host = params.get('trace_config');
    const newConfig = host ?? '';
    if (this.encodedConfigString !== newConfig) {
      this.traceConfig = this.base64ToUint8Array_(newConfig);
      this.encodedConfigString = newConfig;
    }
    this.trackEventConfig = undefined;

    if (this.traceConfig.length === 0) {
      return;
    }

    try {
      const traceConfigObject = TraceConfig.decode(this.traceConfig);

      const trackEventDataSource = traceConfigObject.dataSources?.find(
          ds => ds.config?.trackEventConfig !== undefined);
      if (trackEventDataSource) {
        this.trackEventConfig = trackEventDataSource.config?.trackEventConfig;
      }
    } catch (e) {
      this.showToast_(`Could not parse trace config: ${e}`);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-recorder': TraceRecorderElement;
  }
}

customElements.define(TraceRecorderElement.is, TraceRecorderElement);

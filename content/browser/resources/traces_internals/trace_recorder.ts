// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from '//resources/js/assert.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';

import {getCss} from './trace_recorder.css.js';
import {getHtml} from './trace_recorder.html.js';
import {TracesBrowserProxy} from './traces_browser_proxy.js';

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
    };
  }

  private browserProxy_: TracesBrowserProxy = TracesBrowserProxy.getInstance();
  // Bound method for router events
  private boundLoadConfigFromUrl_ = this.loadConfigFromUrl_.bind(this);
  // Bound method for onTraceComplete listener
  private boundOnTraceComplete_ = this.onTraceComplete_.bind(this);
  // Property to store the listener ID for onTraceComplete
  private onTraceCompleteListenerId_: number|null = null;

  protected accessor traceConfig: string = '';
  protected accessor toastMessage: string = '';
  // Initialize the tracing state to IDLE.
  protected accessor tracingState: TracingState = TracingState.IDLE;

  override connectedCallback() {
    super.connectedCallback();
    this.loadConfigFromUrl_();
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

  protected get isStopTracingEnabled(): boolean {
    return this.tracingState === TracingState.RECORDING;
  }

  protected get isCloneTraceEnabled(): boolean {
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
    }
  }

  protected async stopTracing_(): Promise<void> {
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
      const decodedBytes: Uint8Array =
          this.base64ToUint8Array_(this.traceConfig);
      bigBuffer = {
        bytes: Array.from(decodedBytes),
      } as BigBuffer;
      return bigBuffer;
    } catch (error) {
      this.showToast_(`Error decoding Base64: ${error}`);
    }
    return bigBuffer;
  }

  private onTraceComplete_(trace: BigBuffer|null): void {
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
    const router = CrRouter.getInstance();
    const configParam = router.getQueryParams().get('trace_config');
    const newConfig = configParam ?? '';
    if (this.traceConfig !== newConfig) {
      this.traceConfig = newConfig;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-recorder': TraceRecorderElement;
  }
}

customElements.define(TraceRecorderElement.is, TraceRecorderElement);

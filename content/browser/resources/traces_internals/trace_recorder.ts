// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';

import type {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from '//resources/js/assert.js';
import {CrRouter} from '//resources/js/cr_router.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {BigBuffer} from '//resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {Token} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import {TraceConfig, TraceConfig_BufferConfig_FillPolicy} from './perfetto_config.js';
import type {ChromiumSamplingHeapProfilerConfig, ChromiumStackSamplingProfilerConfig, DataSourceConfig, TraceConfig_BufferConfig, TrackEventConfig} from './perfetto_config.js';
// clang-format off
// <if expr="is_win">
import type {EtwConfig} from './perfetto_config.js';
// </if>
// clang-format on
import {getCss} from './trace_recorder.css.js';
import {getHtml} from './trace_recorder.html.js';
import {downloadTraceData} from './trace_util.js';
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

// <if expr="is_win">
type EtwProviderType = 'scheduler'|'memory'|'system_io';
const ETW_DATA_SOURCE_NAME = 'org.chromium.etw_system';
// </if>

const SAMPLER_PROFILER_DATA_SOURCE_NAME = 'org.chromium.sampler_profiler';
const NATIVE_HEAP_PROFILER_DATA_SOURCE_NAME =
    'org.chromium.native_heap_profiler';

const BUFFER_SIZE_TICKS: number[] = [4, 8, 16, 32, 64, 128, 256, 512];
const STACK_SAMPLING_INTERVAL_TICKS: number[] =
    [2, 5, 10, 20, 50, 100, 200, 500];
const HEAP_SAMPLING_RATE_TICKS: number[] =
    Array.from({length: 20}, (_, i) => 1 << i);
const HEAP_SAMPLING_INTERVAL_TICKS: number[] = [
  0,
  1,
  2,
  5,
  10,
  20,
  50,
  100,
  200,
  500,
  1000,
  2000,
  5000,
  10000,
  30000,
  60000,
];

function findClosestTickIndex(ticks: number[], value: number): number {
  return ticks.reduce(
      (bestIdx, tick, i) =>
          Math.abs(tick - value) < Math.abs(ticks[bestIdx]! - value) ? i :
                                                                       bestIdx,
      0);
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
      toastMessage: {type: String},
      bufferSizeMb: {type: Number},
      bufferFillPolicy: {type: Number},
      tracingState: {type: String},
      trackEventCategories: {type: Array},
      trackEventTags: {type: Array},
      privacyFilterEnabled_: {type: Boolean},
      enabledCategories: {type: Object},
      enabledTags: {type: Object},
      // <if expr="is_win">
      etwEvents: {type: Array},
      enabledEtwEvents: {type: Object},
      etwExpanded_: {type: Boolean},
      // </if>
      disabledTags: {type: Object},
      buffersExpanded_: {type: Boolean},
      categoriesExpanded_: {type: Boolean},
      tagsExpanded_: {type: Boolean},
      stackSamplingExpanded_: {type: Boolean},
      stackSamplingEnabled: {type: Boolean},
      stackSamplingIntervalMs: {type: Number},
      heapProfilingExpanded_: {type: Boolean},
      heapProfilingEnabled: {type: Boolean},
      heapSamplingIntervalKb: {type: Number},
      heapSamplingIntervalMs: {type: Number},
      bufferSizeTicks_: {type: Array},
      stackSamplingIntervalTicks_: {type: Array},
      heapSamplingRateTicks_: {type: Array},
      heapSamplingIntervalTicks_: {type: Array},
      bufferUsage: {type: Number},
      hadDataLoss: {type: Boolean},
      encodedConfigString: {type: String},
      commandLineExpanded_: {type: Boolean},
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
  protected accessor encodedConfigString: string = '';
  protected accessor commandLineExpanded_: boolean = false;

  protected accessor toastMessage: string = '';

  // Initialize the tracing state to IDLE.
  protected accessor tracingState: TracingState = TracingState.IDLE;

  protected accessor trackEventCategories: TraceCategory[] = [];
  protected accessor trackEventTags: string[] = [];

  protected accessor privacyFilterEnabled_: boolean = false;

  protected accessor bufferSizeMb: number = 200;
  protected accessor bufferFillPolicy: TraceConfig_BufferConfig_FillPolicy =
      TraceConfig_BufferConfig_FillPolicy.RING_BUFFER;

  protected traceConfig: TraceConfig|undefined;
  protected trackEventConfig: TrackEventConfig|undefined;
  protected accessor enabledCategories: Set<string> = new Set();
  protected accessor enabledTags: Set<string> = new Set();
  protected accessor disabledTags: Set<string> = new Set();

  // <if expr="is_win">
  protected etwConfig: EtwConfig|undefined;

  protected accessor etwEvents: Array<{
    name: string,
    keyword: string,
    provider: EtwProviderType,
    description: string,
  }> = [];
  protected accessor enabledEtwEvents:
      {[provider in EtwProviderType]: Set<string>} = {
        'memory': new Set(),
        'scheduler': new Set(),
        'system_io': new Set(),
      };
  protected accessor etwExpanded_: boolean = false;
  // </if>

  protected accessor buffersExpanded_: boolean = false;
  protected accessor categoriesExpanded_: boolean = false;
  protected accessor tagsExpanded_: boolean = false;

  protected stackSamplingConfig: ChromiumStackSamplingProfilerConfig|undefined;
  protected accessor stackSamplingExpanded_: boolean = false;
  protected accessor stackSamplingEnabled: boolean = false;
  protected accessor stackSamplingIntervalMs: number = 50;

  protected heapProfilingConfig: ChromiumSamplingHeapProfilerConfig|undefined;
  protected accessor heapProfilingExpanded_: boolean = false;
  protected accessor heapProfilingEnabled: boolean = false;
  protected accessor heapSamplingIntervalKb: number = 128;
  protected accessor heapSamplingIntervalMs: number = 50;

  protected accessor bufferSizeTicks_: number[] = BUFFER_SIZE_TICKS;
  protected accessor stackSamplingIntervalTicks_: number[] =
      STACK_SAMPLING_INTERVAL_TICKS;
  protected accessor heapSamplingRateTicks_: number[] =
      HEAP_SAMPLING_RATE_TICKS;
  protected accessor heapSamplingIntervalTicks_: number[] =
      HEAP_SAMPLING_INTERVAL_TICKS;

  protected getTickIndex_(ticks: number[], value: number): number {
    return findClosestTickIndex(ticks, value);
  }

  protected accessor bufferUsage: number = 0;
  protected accessor hadDataLoss: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.loadConfigFromUrl_();
    this.loadEventCategories_();
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

  protected isStartTracingEnabled(): boolean {
    return this.tracingState === TracingState.IDLE && !!this.traceConfig;
  }

  protected isRecording(): boolean {
    return this.tracingState === TracingState.RECORDING;
  }

  protected getStatusClass(): string {
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

  protected async onStartTracingClick_(): Promise<void> {
    const bigBufferConfig = this.serializeTraceConfigToBigBuffer_();
    if (!bigBufferConfig) {
      return;
    }

    // Set state to RECORDING immediately to disable start button.
    this.tracingState = TracingState.STARTING;

    const {success} = await this.browserProxy_.handler.startTraceSession(
        bigBufferConfig, this.privacyFilterEnabled_);

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

  protected async onStopTracingClick_(): Promise<void> {
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

  protected async onCloneTraceSessionClick_(): Promise<void> {
    const {trace, uuid} = await this.browserProxy_.handler.cloneTraceSession();
    this.downloadData_(trace, uuid);
  }

  protected onPrivacyFilterChange_(event: CustomEvent<boolean>) {
    if (this.privacyFilterEnabled_ === event.detail) {
      return;
    }
    this.privacyFilterEnabled_ = event.detail;
  }

  protected onBuffersExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.buffersExpanded_ = e.detail.value;
  }

  protected onCategoriesExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.categoriesExpanded_ = e.detail.value;
  }

  protected onTagsExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.tagsExpanded_ = e.detail.value;
  }

  protected onCommandLineExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.commandLineExpanded_ = e.detail.value;
  }

  protected getCommandLine_(): string {
    return `--trace-perfetto-config=${
        this.encodedConfigString} --trace-startup-duration=5`;
  }

  protected async onCopyCommandLineClick_(): Promise<void> {
    await navigator.clipboard.writeText(this.getCommandLine_());
    this.showToast_('Command line copied to clipboard');
  }

  protected isCategoryEnabled(category: TraceCategory): boolean {
    if (this.enabledCategories.has(category.name)) {
      return true;
    }
    return this.isCategoryForced(category);
  }

  protected canonicalCategoryName(category: TraceCategory): string {
    const disabledPrefix = 'disabled-by-default-';
    if (category.name.startsWith(disabledPrefix)) {
      const mainPart = category.name.substring(disabledPrefix.length);
      return `${mainPart} (disabled-by-default)`;
    }
    return category.name;
  }

  protected isCategoryForced(category: TraceCategory): boolean {
    for (const tag of category.tags) {
      if (this.disabledTags.has(tag)) {
        return false;
      }
      if (this.enabledTags.has(tag)) {
        return true;
      }
    }
    return false;
  }

  protected isTagEnabled(tagName: string): boolean {
    return this.enabledTags.has(tagName);
  }

  protected isTagDisabled(tagName: string): boolean {
    return this.disabledTags.has(tagName);
  }

  protected onBufferSizeCrSliderValueChanged_(e: Event): void {
    this.bufferSizeMb = this.getSliderTickValue_(e);
    this.updateBufferConfigField_('sizeKb', this.bufferSizeMb * 1024);
  }

  protected onBufferFillPolicyChange_(e: Event) {
    const selectElement = e.target as HTMLSelectElement;
    const policyValue =
        Number(selectElement.value) as TraceConfig_BufferConfig_FillPolicy;

    this.bufferFillPolicy = policyValue;

    this.updateBufferConfigField_('fillPolicy', policyValue);
  }

  private updateBufferConfigField_<K extends keyof TraceConfig_BufferConfig>(
      field: K, value: TraceConfig_BufferConfig[K]): void {
    if (!this.traceConfig?.buffers?.[0]) {
      return;
    }
    this.traceConfig.buffers[0][field] = value;
    this.updateUrlFromConfig_();
  }

  protected onCategoryChange_(event: Event): void {
    if (!this.trackEventConfig) {
      return;
    }
    const isChecked = (event.target as HTMLInputElement).checked;
    const categoryName = (event.currentTarget as HTMLElement).dataset['name']!;

    if (isChecked) {
      this.enabledCategories.add(categoryName);
    } else {
      this.enabledCategories.delete(categoryName);
    }
    this.trackEventConfig.enabledCategories = [...this.enabledCategories];
    // Reset property to force UI update.
    this.enabledCategories = new Set(this.trackEventConfig.enabledCategories);

    this.updateUrlFromConfig_();
  }

  // <if expr="is_win">
  protected onEtwExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.etwExpanded_ = e.detail.value;
  }

  protected isEtwEventEnabled(provider: EtwProviderType, keyword: string):
      boolean {
    return this.enabledEtwEvents[provider].has(keyword);
  }

  protected onEtwEVentChange_(event: CustomEvent<boolean>) {
    const index = Number((event.currentTarget as HTMLElement).dataset['index']);
    const provider = this.etwEvents[index]!.provider;
    const keyword = this.etwEvents[index]!.keyword;

    if (!this.traceConfig) {
      return;
    }
    const isChecked = event.detail;

    if (isChecked) {
      this.enabledEtwEvents[provider].add(keyword);
    } else {
      this.enabledEtwEvents[provider].delete(keyword);
    }
    if (Object.values(this.enabledEtwEvents)
            .every((value) => value.size === 0)) {
      this.enabledEtwEvents[provider] = new Set();
      this.etwConfig = undefined;
      this.removeDataSourceConfig_(ETW_DATA_SOURCE_NAME, 'etwConfig');
    } else {
      if (!this.etwConfig) {
        this.etwConfig = {};
        this.addDataSourceConfig_({
          name: ETW_DATA_SOURCE_NAME,
          targetBuffer: 0,
          etwConfig: this.etwConfig,
        });
      }
      this.enabledEtwEvents[provider] =
          new Set(this.enabledEtwEvents[provider]);
      this.etwConfig.schedulerProviderEvents =
          [...this.enabledEtwEvents['scheduler']];
      this.etwConfig.memoryProviderEvents =
          [...this.enabledEtwEvents['memory']];
      this.etwConfig.systemIoProviderEvents =
          [...this.enabledEtwEvents['system_io']];
    }

    this.updateUrlFromConfig_();
  }
  // </if>

  protected onTagsTrueChange_(event: Event) {
    const tagName = (event.currentTarget as HTMLElement).dataset['tag']!;
    this.onTagsChange_(event, tagName, true);
  }

  protected onTagsFalseChange_(event: Event) {
    const tagName = (event.currentTarget as HTMLElement).dataset['tag']!;
    this.onTagsChange_(event, tagName, false);
  }

  private onTagsChange_(event: Event, tagName: string, enabled: boolean): void {
    if (!this.trackEventConfig) {
      return;
    }
    const isChecked = (event.target as HTMLInputElement).checked;

    const primarySet = enabled ? this.enabledTags : this.disabledTags;
    const secondarySet = enabled ? this.disabledTags : this.enabledTags;
    if (isChecked) {
      primarySet.add(tagName);
      secondarySet.delete(tagName);
    } else {
      primarySet.delete(tagName);
    }
    this.trackEventConfig.enabledTags = [...this.enabledTags];
    this.trackEventConfig.disabledTags = [...this.disabledTags];
    // Reset properties to force UI update.
    this.enabledTags = new Set(this.trackEventConfig.enabledTags);
    this.disabledTags = new Set(this.trackEventConfig.disabledTags);

    this.updateUrlFromConfig_();
  }

  protected onStackSamplingExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.stackSamplingExpanded_ = e.detail.value;
  }

  protected onStackSamplingToggleChange_(event: CustomEvent<boolean>) {
    if (!this.traceConfig) {
      return;
    }
    this.stackSamplingEnabled = event.detail;

    if (this.stackSamplingEnabled && !this.stackSamplingConfig) {
      this.stackSamplingConfig = {
        samplingIntervalMs: this.stackSamplingIntervalMs,
      };
      this.addDataSourceConfig_({
        name: SAMPLER_PROFILER_DATA_SOURCE_NAME,
        targetBuffer: 0,
        chromiumStackSamplingProfiler: this.stackSamplingConfig,
      });
    } else if (!this.stackSamplingEnabled) {
      this.stackSamplingConfig = undefined;
      this.removeDataSourceConfig_(
          SAMPLER_PROFILER_DATA_SOURCE_NAME, 'chromiumStackSamplingProfiler');
    }
    this.updateUrlFromConfig_();
  }

  protected onStackSamplingIntervalCrSliderValueChanged_(e: Event): void {
    this.stackSamplingIntervalMs = this.getSliderTickValue_(e);
    if (this.stackSamplingConfig) {
      this.stackSamplingConfig.samplingIntervalMs =
          this.stackSamplingIntervalMs;
      this.updateUrlFromConfig_();
    }
  }

  protected onHeapProfilingExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.heapProfilingExpanded_ = e.detail.value;
  }

  protected onHeapProfilingToggleChange_(event: CustomEvent<boolean>) {
    if (!this.traceConfig) {
      return;
    }
    this.heapProfilingEnabled = event.detail;

    if (this.heapProfilingEnabled && !this.heapProfilingConfig) {
      this.heapProfilingConfig = {
        samplingIntervalBytes: this.heapSamplingIntervalKb * 1024,
        samplingIntervalMs: this.heapSamplingIntervalMs,
      };
      this.addDataSourceConfig_({
        name: NATIVE_HEAP_PROFILER_DATA_SOURCE_NAME,
        targetBuffer: 0,
        chromiumSamplingHeapProfiler: this.heapProfilingConfig,
      });
    } else if (!this.heapProfilingEnabled) {
      this.heapProfilingConfig = undefined;
      this.removeDataSourceConfig_(
          NATIVE_HEAP_PROFILER_DATA_SOURCE_NAME,
          'chromiumSamplingHeapProfiler');
    }
    this.updateUrlFromConfig_();
  }

  protected onHeapSamplingIntervalCrSliderValueChanged_(e: Event): void {
    this.heapSamplingIntervalKb = this.getSliderTickValue_(e);
    if (this.heapProfilingConfig) {
      this.heapProfilingConfig.samplingIntervalBytes =
          this.heapSamplingIntervalKb * 1024;
      this.updateUrlFromConfig_();
    }
  }

  protected onHeapSamplingIntervalMsCrSliderValueChanged_(e: Event): void {
    this.heapSamplingIntervalMs = this.getSliderTickValue_(e);
    if (this.heapProfilingConfig) {
      this.heapProfilingConfig.samplingIntervalMs = this.heapSamplingIntervalMs;
      this.updateUrlFromConfig_();
    }
  }

  private downloadData_(traceData: BigBuffer|null, uuid: Token|null): void {
    if (!traceData || !uuid) {
      this.showToast_('Failed to download trace or no trace data.');
      return;
    }
    try {
      downloadTraceData(traceData, uuid);
    } catch (e) {
      this.showToast_(`Error downloading trace: ${e}`);
    }
  }

  private async loadEventCategories_(): Promise<void> {
    let {categories} =
        await this.browserProxy_.handler.getTrackEventCategories();

    // Filter category groups.
    categories = categories.filter(category => !category.isGroup);

    // Create a map to get unique categories by name, keeping the last one
    // found.
    categories = Array.from(categories
                                .reduce(
                                    (map, category) => {
                                      map.set(category.name, category);
                                      return map;
                                    },
                                    new Map<string, TraceCategory>())
                                .values());

    // Sort the unique categories and assign them.
    const disabledPrefix = 'disabled-by-default-';
    this.trackEventCategories = categories.sort(
        (a, b) => a.name.replace(disabledPrefix, '')
                      .localeCompare(b.name.replace(disabledPrefix, '')));

    // Extract unique tags using flatMap and a Set.
    this.trackEventTags =
        [...new Set(categories.map(category => category.tags).flat())];

    // <if expr="is_win">
    this.etwEvents = [
      {
        name: 'Context switch',
        keyword: 'CONTEXT_SWITCH',
        provider: 'scheduler',
        description: 'Enables context switch events',
      },
      {
        name: 'Ready Thread',
        keyword: 'DISPATCHER',
        provider: 'scheduler',
        description: 'Enables ready thread events',
      },
      {
        name: 'Memory Counters',
        keyword: 'MEMINFO',
        provider: 'memory',
        description: 'Enables memory counters (free list, zero list, etc.)',
      },
      {
        name: 'File I/O',
        keyword: 'FILE_IO',
        provider: 'system_io',
        description: 'Enables file I/O events',
      },
      {
        name: 'Disk I/O',
        keyword: 'DISK_IO',
        provider: 'system_io',
        description: 'Enables disk I/O events',
      },
    ];
    // </if>
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

  // Encode from Uint8Array into a Base64 string.
  private uint8ArrayToBase64_(bytes: Uint8Array): string {
    let binary = '';
    for (const byte of bytes) {
      binary += String.fromCharCode(byte);
    }
    return btoa(binary);
  }

  private serializeTraceConfigToBigBuffer_(): BigBuffer|undefined {
    if (!this.traceConfig) {
      return;
    }
    let bigBuffer: BigBuffer|undefined = undefined;
    try {
      const serializedConfig = TraceConfig.encode(this.traceConfig).finish();
      bigBuffer = {
        bytes: Array.from(serializedConfig),
      } as BigBuffer;
      return bigBuffer;
    } catch (error) {
      this.showToast_(`Error encoding: ${error}`);
    }
    return bigBuffer;
  }

  private onTraceComplete_(trace: BigBuffer|null, uuid: Token|null): void {
    if (this.bufferPollIntervalId_ !== null) {
      window.clearInterval(this.bufferPollIntervalId_);
      this.bufferPollIntervalId_ = null;
    }
    this.bufferUsage = 0;
    this.hadDataLoss = false;

    this.downloadData_(trace, uuid);

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

    if (this.encodedConfigString === newConfig && newConfig !== '') {
      return;
    }

    this.encodedConfigString = newConfig;
    const serializedConfig = this.base64ToUint8Array_(newConfig);

    if (serializedConfig.length === 0) {
      this.initializeDefaultConfig_();
    } else {
      try {
        this.traceConfig = TraceConfig.decode(serializedConfig);

        // Fallback for the buffer config if the loaded one is missing it.
        if (!this.traceConfig.buffers ||
            this.traceConfig.buffers.length === 0) {
          this.traceConfig.buffers = this.createDefaultBufferConfig_();
        }

        const trackEventDataSource = this.traceConfig.dataSources?.find(
            ds => ds.config?.trackEventConfig !== undefined);
        if (trackEventDataSource) {
          this.trackEventConfig = trackEventDataSource.config?.trackEventConfig;
        } else {
          this.trackEventConfig = this.createDefaultTrackEventConfig_();
          this.addDataSourceConfig_({
            name: 'track_event',
            targetBuffer: 0,
            trackEventConfig: this.trackEventConfig,
          });
        }

        // <if expr="is_win">
        this.etwConfig =
            this.getOrInitDataSourceConfig_(ETW_DATA_SOURCE_NAME, 'etwConfig');
        // </if>

        this.stackSamplingConfig = this.getOrInitDataSourceConfig_(
            SAMPLER_PROFILER_DATA_SOURCE_NAME, 'chromiumStackSamplingProfiler');
        this.heapProfilingConfig = this.getOrInitDataSourceConfig_(
            NATIVE_HEAP_PROFILER_DATA_SOURCE_NAME,
            'chromiumSamplingHeapProfiler');
      } catch (e) {
        this.showToast_(`Could not parse trace config: ${e}`);
        this.initializeDefaultConfig_();
      }
    }
    // Centralized calls to sync the UI and update the URL.
    this.updatePropertiesFromConfig_();
  }

  private updateUrlFromConfig_(): void {
    if (!this.traceConfig) {
      return;
    }
    try {
      // Encode the modified config object back to a Uint8Array
      const writer = TraceConfig.encode(this.traceConfig);

      // Convert to Base64 and update the URL
      const newEncodedConfigString = this.uint8ArrayToBase64_(writer.finish());
      if (this.encodedConfigString === newEncodedConfigString) {
        return;
      }
      this.encodedConfigString = newEncodedConfigString;
      const newUrl = new URL(window.location.href);
      newUrl.searchParams.set('trace_config', newEncodedConfigString);

      // Update URL without reloading the page
      history.replaceState({}, '', newUrl.toString());
    } catch (e) {
      this.showToast_(`Could not update trace config: ${e}`);
    }
  }

  private initializeDefaultConfig_(): void {
    this.stackSamplingConfig = undefined;
    this.heapProfilingConfig = undefined;
    this.trackEventConfig = this.createDefaultTrackEventConfig_();
    this.traceConfig = {
      buffers: this.createDefaultBufferConfig_(),
      dataSources: [
        // DataSource for track events
        {
          config: {
            name: 'track_event',
            targetBuffer: 0,
            trackEventConfig: this.trackEventConfig,
          },
        },
        // DataSource for org.chromium.trace_metadata2
        {
          config: {
            name: 'org.chromium.trace_metadata2',
            targetBuffer: 1,
          },
        },
      ],
    };
  }

  private createDefaultBufferConfig_(): TraceConfig_BufferConfig[] {
    return [
      {
        sizeKb: 200 * 1024,
        fillPolicy: TraceConfig_BufferConfig_FillPolicy.RING_BUFFER,
      },
      {
        sizeKb: 256,
        fillPolicy: TraceConfig_BufferConfig_FillPolicy.DISCARD,
      },
    ];
  }

  private createDefaultTrackEventConfig_(): TrackEventConfig {
    return {
      enabledCategories: [],
      disabledCategories: ['*'],
      enabledTags: [],
      disabledTags: ['slow', 'debug', 'sensitive'],
    };
  }

  private addDataSourceConfig_(config: DataSourceConfig) {
    if (!this.traceConfig) {
      return;
    }
    if (!this.traceConfig.dataSources) {
      this.traceConfig.dataSources = [];
    }
    this.traceConfig.dataSources.push({config: config});
  }

  private removeDataSourceConfig_(
      name: string, fieldKey?: keyof DataSourceConfig): void {
    if (!this.traceConfig?.dataSources) {
      return;
    }
    this.traceConfig.dataSources = this.traceConfig.dataSources.filter(
        ds => ds.config?.name !== name &&
            (!fieldKey || ds.config?.[fieldKey] === undefined));
  }

  private getOrInitDataSourceConfig_<K extends keyof DataSourceConfig>(
      name: string, fieldKey: K): NonNullable<DataSourceConfig[K]>|undefined {
    const dataSource = this.traceConfig?.dataSources?.find(
        ds => ds.config?.[fieldKey] !== undefined || ds.config?.name === name);
    if (!dataSource) {
      return undefined;
    }
    assert(dataSource.config);
    if (!dataSource.config[fieldKey]) {
      dataSource.config[fieldKey] = {} as NonNullable<DataSourceConfig[K]>;
    }
    return dataSource.config[fieldKey] as NonNullable<DataSourceConfig[K]>;
  }

  private getSliderTickValue_(e: Event): number {
    const slider = e.target as CrSliderElement;
    return (slider.ticks as number[])[slider.value]!;
  }

  private updatePropertiesFromConfig_(): void {
    const mainBuffer = this.traceConfig?.buffers?.[0];
    if (mainBuffer) {
      this.bufferSizeMb = Math.floor((mainBuffer.sizeKb ?? 0) / 1024);

      this.bufferFillPolicy = mainBuffer.fillPolicy ??
          TraceConfig_BufferConfig_FillPolicy.RING_BUFFER;
    }

    // Sync the tag and category UI state.
    this.enabledCategories = new Set(this.trackEventConfig?.enabledCategories);
    this.enabledTags = new Set(this.trackEventConfig?.enabledTags);
    this.disabledTags = new Set(this.trackEventConfig?.disabledTags);

    if (this.stackSamplingConfig) {
      this.stackSamplingEnabled = true;
      this.stackSamplingIntervalMs =
          this.stackSamplingConfig.samplingIntervalMs ?? 50;
    } else {
      this.stackSamplingEnabled = false;
    }

    if (this.heapProfilingConfig) {
      this.heapProfilingEnabled = true;
      this.heapSamplingIntervalKb = Math.floor(
          (this.heapProfilingConfig.samplingIntervalBytes ?? (1024 * 128)) /
          1024);
      this.heapSamplingIntervalMs =
          this.heapProfilingConfig.samplingIntervalMs ?? 50;
    } else {
      this.heapProfilingEnabled = false;
    }

    // <if expr="is_win">
    if (this.etwConfig) {
      this.enabledEtwEvents['scheduler'] =
          new Set(this.etwConfig.schedulerProviderEvents);
      this.enabledEtwEvents['memory'] =
          new Set(this.etwConfig.memoryProviderEvents);
      this.enabledEtwEvents['system_io'] =
          new Set(this.etwConfig.systemIoProviderEvents);
    }
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'trace-recorder': TraceRecorderElement;
  }
}

customElements.define(TraceRecorderElement.is, TraceRecorderElement);

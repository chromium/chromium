// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * This class provides a 'bridge' for communicating between javascript and the
 * browser. When run outside of WebUI, e.g. as a regular webpage, it provides
 * synthetic data to assist in testing.
 */
export class BrowserBridge extends EventTarget {
  constructor() {
    super();
    this.clientInfo_ = null;
    this.gpuInfo_ = null;
    this.logMessages_ = [];

    // Request initial gpu info from the C++ backend.
    sendWithPromise('getGpuInfo').then(this.onGpuInfoUpdate_.bind(this));

    // Register a listener to receive future gpu info updates.
    addWebUiListener('gpu-info-updated', this.onGpuInfoUpdate_.bind(this));

    this.updateClientInfo_();
    this.updateLogMessages_();
  }

  dispatchEvent_(eventName) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true}));
  }

  applySimulatedData_(data) {
    // set up things according to the simulated data
    this.gpuInfo_ = data.gpuInfo;
    this.clientInfo_ = data.clientInfo;
    this.logMessages_ = data.logMessages;
    this.dispatchEvent_('gpuInfoUpdate');
    this.dispatchEvent_('clientInfoChange');
    this.dispatchEvent_('logMessagesChange');
  }

  /**
   * Get gpuInfo data.
   */
  get gpuInfo() {
    return this.gpuInfo_;
  }

  /**
   * Called when GPU Info is updated.
   */
  onGpuInfoUpdate_(gpuInfo) {
    this.gpuInfo_ = gpuInfo;
    this.dispatchEvent_('gpuInfoUpdate');
  }

  /**
   * This function begins a request for the ClientInfo. If it comes back
   * as undefined, then we will issue the request again in 250ms.
   */
  async updateClientInfo_() {
    const data = await sendWithPromise('getClientInfo');

    if (data === undefined) {  // try again in 250 ms
      window.setTimeout(this.updateClientInfo_.bind(this), 250);
    } else {
      this.clientInfo_ = data;
      this.dispatchEvent_('clientInfoChange');
    }
  }

  /**
   * Returns information about the currently running Chrome build.
   */
  get clientInfo() {
    return this.clientInfo_;
  }

  /**
   * This function checks for new GPU_LOG messages.
   * If any are found, a refresh is triggered.
   */
  async updateLogMessages_() {
    const messages = await sendWithPromise('getLogMessages');

    if (messages.length !== this.logMessages_.length) {
      this.logMessages_ = messages;
      this.dispatchEvent_('logMessagesChange');
    }
    // check again in 250 ms
    window.setTimeout(this.updateLogMessages_.bind(this), 250);
  }

  /**
   * Returns an array of log messages issued by the GPU process, if any.
   */
  get logMessages() {
    return this.logMessages_;
  }

  /**
   * Returns the value of the "Sandboxed" row.
   */
  isSandboxedForTesting() {
    for (const info of this.gpuInfo_.basicInfo) {
      if (info.description === 'Sandboxed') {
        return info.value;
      }
    }
    return false;
  }
}

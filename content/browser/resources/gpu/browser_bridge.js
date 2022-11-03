// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class provides a 'bridge' for communicating between javascript and the
 * browser. When run outside of WebUI, e.g. as a regular webpage, it provides
 * synthetic data to assist in testing.
 */
export class BrowserBridge extends EventTarget {
  constructor() {
    super();
    this.nextRequestId_ = 0;
    this.pendingCallbacks_ = [];
    this.logMessages_ = [];

    // Tell c++ code that we are ready to receive GPU Info.
    chrome.send('browserBridgeInitialized');
    this.beginRequestClientInfo_();
    this.beginRequestLogMessages_();
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
   * Sends a message to the browser with specified args. The
   * browser will reply asynchronously via the provided callback.
   */
  callAsync(submessage, args, callback) {
    const requestId = this.nextRequestId_;
    this.nextRequestId_ += 1;
    this.pendingCallbacks_[requestId] = callback;
    if (!args) {
      chrome.send('callAsync', [requestId.toString(), submessage]);
    } else {
      const allArgs = [requestId.toString(), submessage].concat(args);
      chrome.send('callAsync', allArgs);
    }
  }

  /**
   * Called by gpu c++ code when client info is ready.
   */
  onCallAsyncReply(requestId, args) {
    if (this.pendingCallbacks_[requestId] === undefined) {
      throw new Error('requestId ' + requestId + ' is not pending');
    }
    const callback = this.pendingCallbacks_[requestId];
    callback(args);
    delete this.pendingCallbacks_[requestId];
  }

  /**
   * Get gpuInfo data.
   */
  get gpuInfo() {
    return this.gpuInfo_;
  }

  /**
   * Called from gpu c++ code when GPU Info is updated.
   */
  onGpuInfoUpdate(gpuInfo) {
    this.gpuInfo_ = gpuInfo;
    this.dispatchEvent_('gpuInfoUpdate');
  }

  /**
   * This function begins a request for the ClientInfo. If it comes back
   * as undefined, then we will issue the request again in 250ms.
   */
  beginRequestClientInfo_() {
    this.callAsync(
        'requestClientInfo', undefined,
        (function(data) {
          if (data === undefined) {  // try again in 250 ms
            window.setTimeout(this.beginRequestClientInfo_.bind(this), 250);
          } else {
            this.clientInfo_ = data;
            this.dispatchEvent_('clientInfoChange');
          }
        }).bind(this));
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
  beginRequestLogMessages_() {
    this.callAsync(
        'requestLogMessages', undefined,
        (function(messages) {
          if (messages.length !== this.logMessages_.length) {
            this.logMessages_ = messages;
            this.dispatchEvent_('logMessagesChange');
          }
          // check again in 250 ms
          window.setTimeout(this.beginRequestLogMessages_.bind(this), 250);
        }).bind(this));
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

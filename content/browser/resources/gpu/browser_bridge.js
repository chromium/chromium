// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('gpu', function() {
  /**
   * This class provides a 'bridge' for communicating between javascript and the
   * browser. When run outside of WebUI, e.g. as a regular webpage, it provides
   * synthetic data to assist in testing.
   */
  class BrowserBridge extends cr.EventTarget {
    constructor() {
      super();
      // If we are not running inside WebUI, output chrome.send messages
      // to the console to help with quick-iteration debugging.
      this.debugMode_ = (chrome.send === undefined && console.log);
      if (this.debugMode_) {
        const browserBridgeTests = document.createElement('script');
        browserBridgeTests.src = './gpu_internals/browser_bridge_tests.js';
        document.body.appendChild(browserBridgeTests);
      }

      this.nextRequestId_ = 0;
      this.pendingCallbacks_ = [];
      this.logMessages_ = [];

      // Tell c++ code that we are ready to receive GPU Info.
      if (!this.debugMode_) {
        chrome.send('browserBridgeInitialized');
        this.beginRequestClientInfo_();
        this.beginRequestLogMessages_();
      }
    }

    applySimulatedData_(data) {
      // set up things according to the simulated data
      this.gpuInfo_ = data.gpuInfo;
      this.clientInfo_ = data.clientInfo;
      this.logMessages_ = data.logMessages;
      cr.dispatchSimpleEvent(this, 'gpuInfoUpdate');
      cr.dispatchSimpleEvent(this, 'clientInfoChange');
      cr.dispatchSimpleEvent(this, 'logMessagesChange');
    }

    /**
     * Returns true if the page is hosted inside Chrome WebUI
     * Helps have behavior conditional to emulate_webui.py
     */
    get debugMode() {
      return this.debugMode_;
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
      cr.dispatchSimpleEvent(this, 'gpuInfoUpdate');
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
              cr.dispatchSimpleEvent(this, 'clientInfoChange');
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
            if (messages.length != this.logMessages_.length) {
              this.logMessages_ = messages;
              cr.dispatchSimpleEvent(this, 'logMessagesChange');
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
      for (i = 0; i < this.gpuInfo_.basicInfo.length; ++i) {
        const info = this.gpuInfo_.basicInfo[i];
        if (info.description == 'Sandboxed') {
          return info.value;
        }
      }
      return false;
    }
  }

  return {BrowserBridge: BrowserBridge};
});

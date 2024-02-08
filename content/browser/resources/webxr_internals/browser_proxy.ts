// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DeviceInfo, RuntimeInfo} from './webxr_internals.mojom-webui.js';
import {WebXrInternalsHandler, XRInternalsSessionListenerCallbackRouter} from './webxr_internals.mojom-webui.js';

export class BrowserProxy {
  private callbackRouter_ = new XRInternalsSessionListenerCallbackRouter();
  private handler_ = WebXrInternalsHandler.getRemote();

  constructor() {
    this.handler_.subscribeToEvents(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
  }

  getBrowserCallback(): XRInternalsSessionListenerCallbackRouter {
    return this.callbackRouter_;
  }

  async getDeviceInfo(): Promise<DeviceInfo> {
    const response = await this.handler_.getDeviceInfo();
    return response.deviceInfo;
  }

  async getActiveRuntimes(): Promise<RuntimeInfo[]> {
    const response = await this.handler_.getActiveRuntimes();
    return response.activeRuntimes;
  }

  static getInstance() {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;

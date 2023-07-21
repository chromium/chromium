// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeviceInfo, WebXrInternalsHandler} from './webxr_internals.mojom-webui.js';

export class BrowserProxy {
  private handler = WebXrInternalsHandler.getRemote();

  async getDeviceInfo(): Promise<DeviceInfo> {
    const response = await this.handler.getDeviceInfo();
    return response.deviceInfo;
  }

  static getInstance() {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;

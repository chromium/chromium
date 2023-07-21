// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './device_info_table.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {BrowserProxy} from './browser_proxy.js';

async function renderDeviceInfo() {
  const browserProxy = BrowserProxy.getInstance();
  assert(browserProxy);

  const deviceInfo = await browserProxy.getDeviceInfo();

  const deviceInfoContent = getRequiredElement('device-info-content');
  assert(deviceInfoContent);

  const table = document.createElement('device-info-table');
  table.addRow('Operating System Name', deviceInfo.operatingSystemName);
  table.addRow('Operating System Version', deviceInfo.operatingSystemVersion);
  table.addRow('GPU Gl Vendor', deviceInfo.gpuGlVendor);
  table.addRow('GPU GL Renderer', deviceInfo.gpuGlRenderer);
  deviceInfoContent.appendChild(table);
}

document.addEventListener('DOMContentLoaded', renderDeviceInfo);

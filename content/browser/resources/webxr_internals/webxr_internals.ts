// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './device_info_table.js';
import './session_info_table.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {BrowserProxy} from './browser_proxy.js';
import {SessionRequestRecord} from './webxr_internals.mojom-webui.js';


let browserProxy: BrowserProxy;

async function bootstrap() {
  browserProxy = BrowserProxy.getInstance();
  assert(browserProxy);

  setupSidebarButtonListeners();
  renderDeviceInfo();
  renderSessionsInfo();
}

async function setupSidebarButtonListeners() {
  const deviceInfoButton = getRequiredElement('device-info-button');
  const sessionInfoButton = getRequiredElement('session-info-button');

  deviceInfoButton.addEventListener('click', () => {
    switchSidebar('device-info');
  });

  sessionInfoButton.addEventListener('click', () => {
    switchSidebar('session-info');
  });
}

function ensureOnlyContentSelected(
    sidebarId: string, querySelectorString: string, elementSuffix: string) {
  const elements = document.body.querySelectorAll(querySelectorString);
  elements.forEach(element => element.classList.remove('active'));
  const selectedElement = getRequiredElement(sidebarId + elementSuffix);
  selectedElement.classList.add('active');
}

function switchSidebar(sidebarId: string) {
  ensureOnlyContentSelected(sidebarId, '.tab-content', '-content');
  ensureOnlyContentSelected(sidebarId, '.tab-button', '-button');
}

async function renderDeviceInfo() {
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

async function renderSessionsInfo() {
  const sessionInfoConent = getRequiredElement('session-info-content');
  assert(sessionInfoConent);

  const table = document.createElement('session-info-table');

  browserProxy.getBrowserCallback().addXrSessionRequest.addListener(
      (sessionRequestRecord: SessionRequestRecord) => {
        table.addRow(sessionRequestRecord);
      });

  sessionInfoConent.appendChild(table);
}

document.addEventListener('DOMContentLoaded', bootstrap);

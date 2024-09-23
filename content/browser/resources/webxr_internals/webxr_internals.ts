// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './active_runtime_info_table.js';
import './device_info_table.js';
import './runtime_changelog_table.js';
import './session_info_table.js';
import './session_statistics_table.js';

import {assert} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {ActiveRuntimeInfoTableElement} from './active_runtime_info_table.js';
import {BrowserProxy} from './browser_proxy.js';
import type {RuntimeInfo, SessionRejectedRecord, SessionRequestedRecord, SessionStartedRecord, SessionStoppedRecord} from './webxr_internals.mojom-webui.js';
import type {XRDeviceId} from './xr_device.mojom-webui.js';
import type {XrFrameStatistics, XrLogMessage} from './xr_session.mojom-webui.js';

let browserProxy: BrowserProxy;

async function bootstrap() {
  browserProxy = BrowserProxy.getInstance();
  assert(browserProxy);

  setupSidebarButtonListeners();
  renderDeviceInfoContent();
  renderSessionInfoContent();
  renderRuntimeInfoContent();
  renderSessionStatisticsContent();
}

async function setupSidebarButtonListeners() {
  const deviceInfoButton = getRequiredElement('device-info-button');
  const sessionInfoButton = getRequiredElement('session-info-button');
  const runtimeInfoButton = getRequiredElement('runtime-info-button');
  const sessionStatisticsButton =
      getRequiredElement('session-statistics-button');

  deviceInfoButton.addEventListener('click', () => {
    switchSidebar('device-info');
  });

  sessionInfoButton.addEventListener('click', () => {
    switchSidebar('session-info');
  });

  runtimeInfoButton.addEventListener('click', () => {
    switchSidebar('runtime-info');
  });

  sessionStatisticsButton.addEventListener('click', () => {
    switchSidebar('session-statistics');
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

async function renderDeviceInfoContent() {
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

async function renderSessionInfoContent() {
  const sessionInfoContent = getRequiredElement('session-info-content');
  assert(sessionInfoContent);

  const table = document.createElement('session-info-table');

  browserProxy.getBrowserCallback().logXrSessionRequested.addListener(
      (sessionRequestedRecord: SessionRequestedRecord) => {
        table.addSessionRequestedRow(sessionRequestedRecord);
      });

  browserProxy.getBrowserCallback().logXrSessionRejected.addListener(
      (sessionRejectedRecord: SessionRejectedRecord) => {
        table.addSessionRejectedRow(sessionRejectedRecord);
      });

  browserProxy.getBrowserCallback().logXrSessionStarted.addListener(
      (sessionStartedRecord: SessionStartedRecord) => {
        table.addSessionStartedRow(sessionStartedRecord);
      });

  browserProxy.getBrowserCallback().logXrSessionStopped.addListener(
      (sessionStoppedRecord: SessionStoppedRecord) => {
        table.addSessionStoppedRow(sessionStoppedRecord);
      });

  sessionInfoContent.appendChild(table);
}

async function renderActiveRuntimesTable(
    runtimeInfoTable: ActiveRuntimeInfoTableElement) {
  const activeRuntimes = await browserProxy.getActiveRuntimes();
  runtimeInfoTable.recreateActiveRuntimesTable(activeRuntimes);
}

async function renderRuntimeInfoContent() {
  const runtimeInfoContent = getRequiredElement('runtime-info-content');
  assert(runtimeInfoContent);

  const activeRuntimeTable =
      document.createElement('active-runtime-info-table');
  const runtimeChangelogTable =
      document.createElement('runtime-changelog-table');

  renderActiveRuntimesTable(activeRuntimeTable);

  browserProxy.getBrowserCallback().logXrRuntimeAdded.addListener(
      (runtimeInfo: RuntimeInfo) => {
        runtimeChangelogTable.addRuntimeAddedRecord(runtimeInfo);
        renderActiveRuntimesTable(activeRuntimeTable);
      });

  browserProxy.getBrowserCallback().logXrRuntimeRemoved.addListener(
      (deviceId: XRDeviceId) => {
        runtimeChangelogTable.addRuntimeRemovedRecord(deviceId);
        renderActiveRuntimesTable(activeRuntimeTable);
      });

  runtimeInfoContent.appendChild(activeRuntimeTable);
  runtimeInfoContent.appendChild(runtimeChangelogTable);
}

async function renderSessionStatisticsContent() {
  const sessionStatisticsContent =
      getRequiredElement('session-statistics-content');
  assert(sessionStatisticsContent);

  const table = document.createElement('session-statistics-table');

  browserProxy.getBrowserCallback().logConsoleMessages.addListener(
      (xrLogMessage: XrLogMessage) => {
        table.addConsoleMessageRow(xrLogMessage);
      });

  sessionStatisticsContent.appendChild(table);

  browserProxy.getBrowserCallback().logFrameData.addListener(
      (xrSessionStatistics: XrFrameStatistics) => {
        table.addXrSessionStatisticsRow(xrSessionStatistics);
      });

  sessionStatisticsContent.appendChild(table);
}

document.addEventListener('DOMContentLoaded', bootstrap);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './runtime_changelog_table.html.js';
import * as TimeUtil from './time_util.js';
import type {RuntimeInfo} from './webxr_internals.mojom-webui.js';
import type {XRDeviceId} from './xr_device.mojom-webui.js';
import * as XRRuntimeUtil from './xr_runtime_util.js';
import * as XRSessionUtil from './xr_session_util.js';

const ADDED_RUNTIME_STATE: string = 'Added';
const REMOVED_RUNTIME_STATE: string = 'Removed';

export class RuntimeChangelogTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  addRuntimeAddedRecord(runtimeInfo: RuntimeInfo) {
    const {deviceId, supportedFeatures, isArBlendModeSupported} = runtimeInfo;
    const cellValues = [
      XRRuntimeUtil.deviceIdToString(deviceId),
      ADDED_RUNTIME_STATE,
      TimeUtil.getCurrentDateTime(),
      supportedFeatures.map(XRSessionUtil.sessionFeatureToString).join(', '),
      isArBlendModeSupported.toString(),
    ];

    this.addRow(cellValues);
  }

  addRuntimeRemovedRecord(deviceId: XRDeviceId) {
    const cellValues = [
      XRRuntimeUtil.deviceIdToString(deviceId),
      REMOVED_RUNTIME_STATE,
      TimeUtil.getCurrentDateTime(),
    ];

    this.addRow(cellValues);
  }

  addRow(cellValues: string[]) {
    const table =
        this.getRequiredElement<HTMLTableElement>('#runtimes-changelog-table');
    const newRow = table.insertRow();

    cellValues.forEach((value) => {
      const cell = newRow.insertCell();
      cell.textContent = value;
    });
  }
}

// Declare the custom element
declare global {
  interface HTMLElementTagNameMap {
    'runtime-changelog-table': RuntimeChangelogTableElement;
  }
}

customElements.define('runtime-changelog-table', RuntimeChangelogTableElement);

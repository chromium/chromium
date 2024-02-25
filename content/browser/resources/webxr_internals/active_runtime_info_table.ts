// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './active_runtime_info_table.html.js';
import type {RuntimeInfo} from './webxr_internals.mojom-webui.js';
import * as XRRuntimeUtil from './xr_runtime_util.js';
import * as XRSessionUtil from './xr_session_util.js';

export class ActiveRuntimeInfoTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  recreateActiveRuntimesTable(activeRuntimesInfo: RuntimeInfo[]) {
    const table =
        this.getRequiredElement<HTMLTableElement>('#active-runtimes-table');

    // Preserve the header row and remove all other rows from the table.
    while (table.rows.length > 1) {
      table.deleteRow(1);
    }

    activeRuntimesInfo.forEach((activeRuntimeInfo) => {
      const {deviceId, supportedFeatures, isArBlendModeSupported} =
          activeRuntimeInfo;
      const cellValues = [
        XRRuntimeUtil.deviceIdToString(deviceId),
        supportedFeatures.map(XRSessionUtil.sessionFeatureToString).join(', '),
        isArBlendModeSupported.toString(),
      ];

      this.addRow(cellValues);
    });
  }

  addRow(cellValues: string[]) {
    const table =
        this.getRequiredElement<HTMLTableElement>('#active-runtimes-table');
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
    'active-runtime-info-table': ActiveRuntimeInfoTableElement;
  }
}

customElements.define(
    'active-runtime-info-table', ActiveRuntimeInfoTableElement);

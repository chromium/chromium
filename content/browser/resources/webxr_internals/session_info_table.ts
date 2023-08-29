// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {getTemplate} from './session_info_table.html.js';
import {SessionRequestRecord} from './webxr_internals.mojom-webui.js';
import {depthFormatToString, depthUsageToString, sessionFeatureToString, sessionModeToString} from './xr_session_util.js';

// TODO(https://crbug.com/1458662): Consider converting this to an object that
// can also handle parsing the value out of the request object
const COLUMN_NAMES = [
  'Trace ID',
  'Mode',
  'Required Features',
  'Optional Features',
  'Depth Usage Preferences',
  'Depth Data Format Preferences',
  'Requested Time',
];

export class SessionInfoTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();

    const table =
        this.getRequiredElement<HTMLTableElement>('#session-info-table');

    const headerRow = table.insertRow();
    COLUMN_NAMES.forEach((columnName) => {
      const headerCell = document.createElement('th');
      headerCell.textContent = columnName;
      headerRow.appendChild(headerCell);
    });
  }

  addRow(sessionRequestRecord: SessionRequestRecord) {
    const {traceId, mode, requiredFeatures, optionalFeatures, depthOptions} =
        sessionRequestRecord.options;

    const cellValues = [
      traceId.toString(),
      sessionModeToString(mode),
      requiredFeatures.map(sessionFeatureToString).join(', '),
      optionalFeatures.map(sessionFeatureToString).join(', '),
      depthOptions?.usagePreferences.map(depthUsageToString).join(', '),
      depthOptions?.dataFormatPreferences.map(depthFormatToString).join(', '),
      formatMojoTime(sessionRequestRecord.requestedTime),
    ];

    const table =
        this.getRequiredElement<HTMLTableElement>('#session-info-table');
    const newRow = table.insertRow();

    cellValues.forEach((value) => {
      const cell = newRow.insertCell();
      if (typeof value === 'object') {
        cell.textContent = JSON.stringify(value);
      } else if (value !== undefined) {
        cell.textContent = value;
      } else {
        cell.textContent = '[]';
      }
    });
  }
}

function formatMojoTime(mojoTime: Time) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return (new Date(timeInMs - epochDeltaInMs)).toLocaleString();
}

// Declare the custom element
declare global {
  interface HTMLElementTagNameMap {
    'session-info-table': SessionInfoTableElement;
  }
}

customElements.define('session-info-table', SessionInfoTableElement);

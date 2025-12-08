// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

function buildTableRow(
    headerName: string, dataKey: string): HTMLTableRowElement {
  const data = loadTimeData.getString(dataKey);

  const tr = document.createElement('tr');

  const th = document.createElement('th');
  th.textContent = headerName;
  tr.appendChild(th);

  const td = document.createElement('td');
  td.textContent = data;
  tr.appendChild(td);

  return tr;
}

function appendRow(
    tableElement: HTMLElement, headerName: string, dataKey: string) {
  if (loadTimeData.valueExists(dataKey)) {
    tableElement.appendChild(buildTableRow(headerName, dataKey));
  }
}

/* All the work we do onload. */
function initialize() {
  const tableEl = getRequiredElement('data-table');
  tableEl.textContent = '';

  appendRow(tableEl, 'Active Program', 'activeProgramName');
  appendRow(tableEl, 'Device Determined Program', 'deviceDeterminedProgram');
  appendRow(tableEl, 'Active Country', 'activeCountryCode');
  appendRow(tableEl, 'Country in prefs', 'prefsCountryCode');
  appendRow(tableEl, 'Recorded Eligibility (static)', 'recordedEligibility');
  appendRow(tableEl, 'External Choice', 'externalChoiceKeyword');
}
document.addEventListener('DOMContentLoaded', initialize);

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


/* All the work we do onload. */
function initialize() {
  const tableElement = getRequiredElement('data-table');
  tableElement.textContent = '';
  tableElement.appendChild(
      buildTableRow('Active Program', 'activeProgramName'));
  tableElement.appendChild(
      buildTableRow('Active Country', 'activeCountryCode'));
  tableElement.appendChild(
      buildTableRow('Country in prefs', 'prefsCountryCode'));
}
document.addEventListener('DOMContentLoaded', initialize);

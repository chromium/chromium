// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {QuotaInternalsHandler, QuotaInternalsHandlerRemote} from './quota_internals.mojom-webui.js';

/**
 * @param response
 */
function renderDiskAvailability(response) {
  const rowTemplate = /** @type {!HTMLTemplateElement} */ ($('listener-row'));
  const tableBody = /** @type {!HTMLTableElement} */ ($('listeners-tbody'));
  const listenerRowTemplate =
      /** @type {!HTMLTemplateElement} */ (rowTemplate.cloneNode(true));
  const listenerRow = listenerRowTemplate.content;

  const availableSpaceBytes =
      parseFloat(Number(response.availableSpace) / (1024 ** 3)).toFixed(2);
  const totalSpaceBytes =
      parseFloat(Number(response.totalSpace) / (1024 ** 3)).toFixed(2);

  tableBody.innerHTML = trustedTypes.emptyHTML;

  listenerRow.querySelector('.total-space').textContent =
      `${totalSpaceBytes} GB`;
  listenerRow.querySelector('.available-space').textContent =
      `${availableSpaceBytes} GB`;

  tableBody.append(listenerRow);
}

/**
 * @param response
 */
function renderEvictionStats(response) {
  const rowTemplate = /** @type {!HTMLTemplateElement} */ ($('eviction-row'));
  const evictionRowTemplate =
      /** @type {!HTMLTemplateElement} */ (rowTemplate.cloneNode(true));
  const evictionRow = evictionRowTemplate.content;

  const tableBody = /** @type {!HTMLTableElement} */ ($('eviction-tbody'));
  tableBody.innerHTML = trustedTypes.emptyHTML;

  evictionRow.querySelector('.errors-on-getting-usage-and-quota').textContent =
      response.evictionStatistics['errors-on-getting-usage-and-quota'];
  evictionRow.querySelector('.evicted-buckets').textContent =
      response.evictionStatistics['evicted-buckets'];
  evictionRow.querySelector('.eviction-rounds').textContent =
      response.evictionStatistics['eviction-rounds'];
  evictionRow.querySelector('.skipped-eviction-rounds').textContent =
      response.evictionStatistics['skipped-eviction-rounds'];

  tableBody.appendChild(evictionRow);
}

document.addEventListener('DOMContentLoaded', () => {
  /**
   * @type {!QuotaInternalsHandlerRemote}
   */
  const pageHandler = QuotaInternalsHandler.getRemote();

  pageHandler.getDiskAvailability().then(renderDiskAvailability);
  pageHandler.getStatistics().then(renderEvictionStats);

});
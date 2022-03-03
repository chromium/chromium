// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {QuotaInternalsBrowserProxy} from './quota_internals_browser_proxy.js';

function getProxy(): QuotaInternalsBrowserProxy {
  return QuotaInternalsBrowserProxy.getInstance();
}

async function renderDiskAvailability() {
  const result = await getProxy().getDiskAvailability();

  const rowTemplate: HTMLTemplateElement =
      document.body.querySelector<HTMLTemplateElement>('#listener-row')!;
  const tableBody: HTMLTableElement =
      document.body.querySelector<HTMLTableElement>('#listeners-tbody')!;
  const listenerRowTemplate: HTMLTemplateElement =
      rowTemplate.cloneNode(true) as HTMLTemplateElement;
  const listenerRow = listenerRowTemplate.content;

  const availableSpaceBytes =
      (Number(result.availableSpace) / (1024 ** 3)).toFixed(2);
  const totalSpaceBytes = (Number(result.totalSpace) / (1024 ** 3)).toFixed(2);

  listenerRow.querySelector('.total-space')!.textContent =
      `${totalSpaceBytes} GB`;
  listenerRow.querySelector('.available-space')!.textContent =
      `${availableSpaceBytes} GB`;

  tableBody.append(listenerRow);
}

async function renderEvictionStats() {
  const result = await getProxy().getStatistics();

  const rowTemplate: HTMLTemplateElement =
      document.body.querySelector<HTMLTemplateElement>('#eviction-row')!;
  const tableBody: HTMLTableElement =
      document.body.querySelector<HTMLTableElement>('#eviction-tbody')!;
  const evictionRowTemplate: HTMLTemplateElement =
      rowTemplate.cloneNode(true) as HTMLTemplateElement;
  const evictionRow = evictionRowTemplate.content;

  evictionRow.querySelector('.errors-on-getting-usage-and-quota')!.textContent =
      result.evictionStatistics['errors-on-getting-usage-and-quota'];
  evictionRow.querySelector('.evicted-buckets')!.textContent =
      result.evictionStatistics['evicted-buckets'];
  evictionRow.querySelector('.eviction-rounds')!.textContent =
      result.evictionStatistics['eviction-rounds'];
  evictionRow.querySelector('.skipped-eviction-rounds')!.textContent =
      result.evictionStatistics['skipped-eviction-rounds'];

  tableBody.appendChild(evictionRow);
}

document.addEventListener('DOMContentLoaded', () => {
  renderDiskAvailability();
  renderEvictionStats();
  document.body.querySelector('#trigger-notification')!.addEventListener(
      'click', () => getProxy().simulateStoragePressure());

  document.body.querySelector('#summary-tab')!.addEventListener('click', () => {
    document.body.querySelector('#usage-tabpanel')!.removeAttribute('selected');
    document.body.querySelector('#summary-tabpanel')!.setAttribute(
        'selected', 'selected');
  });

  document.querySelector('#usage-tab')!.addEventListener('click', () => {
    document.querySelector('#summary-tabpanel')!.removeAttribute('selected');
    document.querySelector('#usage-tabpanel')!.setAttribute(
        'selected', 'selected');
  });
});
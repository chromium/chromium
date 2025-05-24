// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {BucketTableEntry} from './quota_internals.mojom-webui.js';
import type {RetrieveBucketsTableResult} from './quota_internals_browser_proxy.js';
import {QuotaInternalsBrowserProxy} from './quota_internals_browser_proxy.js';

// Object for constructing the bucket row in the usage table.
interface BucketEntry {
  'bucketId': string;
  'name': string;
  'usage': string;
  'useCount': string;
  'lastAccessed': string;
  'lastModified': string;
}

// BucketEntry organized by StorageKey for constructing the usage table.
interface StorageKeyData {
  'bucketCount': number;
  'storageKeyEntries': BucketEntry[];
}

// Map of StorageKey entries for constructing bucket usage table.
interface BucketTableEntriesByStorageKey {
  // key = storageKey
  [key: string]: StorageKeyData;
}

// Converts a mojo time to a JS time.
function convertMojoTimeToJS(mojoTime: Time): Date {
  // The JS Date() is based off of the number of milliseconds since
  // the UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue|
  // of the base::Time (represented in mojom.Time) represents the
  // number of microseconds since the Windows FILETIME epoch
  // (1601-01-01 00:00:00 UTC). This computes the final JS time by
  // computing the epoch delta and the conversion from microseconds to
  // milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to
  // base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

function getProxy(): QuotaInternalsBrowserProxy {
  return QuotaInternalsBrowserProxy.getInstance();
}

async function renderDiskAvailabilityAndTempPoolSize() {
  const result = await getProxy().getDiskAvailabilityAndTempPoolSize();

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
  const tempPoolSizeBytes =
      (Number(result.tempPoolSize) / (1024 ** 3)).toFixed(2);

  listenerRow.querySelector('.total-space')!.textContent =
      `${totalSpaceBytes} GB`;
  listenerRow.querySelector('.available-space')!.textContent =
      `${availableSpaceBytes} GB`;
  listenerRow.querySelector('.temp-pool-size')!.textContent =
      `${tempPoolSizeBytes} GB`;

  tableBody.append(listenerRow);
}

async function renderGlobalUsage() {
  const result = await getProxy().getGlobalUsage();
  const formattedResultString: string = `${Number(result.usage)} B (${
      result.unlimitedUsage} B for unlimited origins)`;
  document.body.querySelector(`.global-and-unlimited-usage`)!.textContent =
      formattedResultString;
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

async function renderUsageAndQuotaStats() {
  const bucketTable: RetrieveBucketsTableResult =
      await getProxy().retrieveBucketsTable();
  const bucketTableEntries: BucketTableEntry[] = bucketTable.entries;
  const bucketTableEntriesByStorageKey: BucketTableEntriesByStorageKey = {};

  /* Re-structure bucketTableEntries data to be accessible by a storage key.
   * bucketTableEntriesByStorageKey = {
   *   <storage_key_string>: {
   *     bucketCount: <number>,
   *     storageKeyEntries: [{
   *         bucketId: <bigint>,
   *         name: <string>,
   *         usage: <bigint>,
   *         useCount: <bigint>,
   *         lastAccessed: <Time>,
   *         lastModified: <Time>
   *        }]
   *      }
   *    }
   */

  for (let i = 0; i < bucketTableEntries.length; i++) {
    const entry = bucketTableEntries[i];
    const bucketTableEntryObj: BucketEntry = {
      bucketId: entry.bucketId.toString(),
      name: entry.name,
      usage: entry.usage.toString(),
      useCount: entry.useCount.toString(),
      lastAccessed: convertMojoTimeToJS(entry.lastAccessed)
                        .toLocaleString('en-US', {timeZoneName: 'short'}),
      lastModified: convertMojoTimeToJS(entry.lastModified)
                        .toLocaleString('en-US', {timeZoneName: 'short'}),
    };

    if (!(entry.storageKey in bucketTableEntriesByStorageKey)) {
      bucketTableEntriesByStorageKey[entry.storageKey] = {
        'bucketCount': 0,
        'storageKeyEntries': [],
      };
    }
    bucketTableEntriesByStorageKey[entry.storageKey]['storageKeyEntries'].push(
        bucketTableEntryObj);
    bucketTableEntriesByStorageKey[entry.storageKey]['bucketCount'] += 1;
  }

  const storageKeys: string[] = Object.keys(bucketTableEntriesByStorageKey);

  /* Populate the rows of the Usage and Quota table by iterating over:
   * each storage key in bucketTableEntriesByStorageKey,
   * each storage key's bucket(s). */

  // Iterate over each storageKey in bucketTableEntriesByStorageKey.
  for (let i = 0; i < storageKeys.length; i++) {
    const storageKey: string = storageKeys[i];
    const storageKeyRowSpan =
        bucketTableEntriesByStorageKey[storageKey]['bucketCount'];
    const buckets: BucketEntry[] =
        bucketTableEntriesByStorageKey[storageKey]['storageKeyEntries'];

    // Iterate over each bucket for a given storageKey.
    for (let k = 0; k < buckets.length; k++) {
      const isFirstStorageKeyRow: boolean = (k === 0);

      // Initialize a Usage and Quota table row template.
      const rowTemplate: HTMLTemplateElement =
          document.body.querySelector<HTMLTemplateElement>(
              '#usage-and-quota-row')!;
      const tableBody: HTMLTableElement =
          document.body.querySelector<HTMLTableElement>(
              '#usage-and-quota-tbody')!;
      const usageAndQuotaRowTemplate: HTMLTemplateElement =
          rowTemplate.cloneNode(true) as HTMLTemplateElement;
      const usageAndQuotaRow = usageAndQuotaRowTemplate.content;

      usageAndQuotaRow.querySelector('.storage-key')!.textContent = storageKey;
      usageAndQuotaRow.querySelector('.storage-key')!.setAttribute(
          'rowspan', `${storageKeyRowSpan}`);
      usageAndQuotaRow.querySelector('.bucket')!.textContent = buckets[k].name;
      usageAndQuotaRow.querySelector('.usage')!.textContent = buckets[k].usage;
      usageAndQuotaRow.querySelector('.use-count')!.textContent =
          buckets[k].useCount;
      usageAndQuotaRow.querySelector('.last-accessed')!.textContent =
          buckets[k].lastAccessed;
      usageAndQuotaRow.querySelector('.last-modified')!.textContent =
          buckets[k].lastModified;

      /* If the current row is not the first of its kind for a given storage
       * key, remove the storage key cell when appending the row to the table
       * body. This creates a nested row to the right of the storage key cell.
       */
      if (!isFirstStorageKeyRow) {
        usageAndQuotaRow.querySelector('.storage-key')!.remove();
      }
      tableBody.appendChild(usageAndQuotaRow);
    }
  }
}

function renderSimulateStoragePressureButton() {
  getProxy().isSimulateStoragePressureAvailable().then(result => {
    if (!result.available) {
      document.body
          .querySelector('#simulate-storage-pressure-activation-message')
          ?.removeAttribute('hidden');
      document.body.querySelector('#trigger-notification')!.setAttribute(
          'disabled', '');
    }
  });

  document.body.querySelector('#trigger-notification')!.addEventListener(
      'click', () => getProxy().simulateStoragePressure());
}

document.addEventListener('DOMContentLoaded', () => {
  renderDiskAvailabilityAndTempPoolSize();
  renderEvictionStats();
  renderGlobalUsage();
  renderUsageAndQuotaStats();
  renderSimulateStoragePressureButton();
});

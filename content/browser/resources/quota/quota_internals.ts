// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {BucketTableEntry} from './quota_internals.mojom-webui.js';
import type {RetrieveBucketsTableResult} from './quota_internals_browser_proxy.js';
import {QuotaInternalsBrowserProxy, StorageType} from './quota_internals_browser_proxy.js';

// Object for constructing the bucket row in the usage table.
interface StorageTypeBucketTableEntry {
  'bucketId': string;
  'name': string;
  'usage': string;
  'useCount': string;
  'lastAccessed': string;
  'lastModified': string;
}

// Bucket entries organized by StorageType for constructing the usage table.
interface StorageTypeEntries {
  // key = storageType
  [key: string]: StorageTypeBucketTableEntry[];
}

// StorageTypeEntries organized by StorageKey for constructing the usage table.
interface StorageKeyData {
  'bucketCount': number;
  'storageKeyEntries': StorageTypeEntries;
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
  const typeVals: number[] =
      Object.keys(StorageType).map((v) => Number(v)).filter((v) => !isNaN(v));

  for (const typeVal of typeVals) {
    const result = await getProxy().getGlobalUsage(typeVal);
    const formattedResultString: string = `${Number(result.usage)} B (${
        result.unlimitedUsage} B for unlimited origins)`;
    document.body
        .querySelector(`.${
            StorageType[typeVal].toLowerCase()}-global-and-unlimited-usage`)!
        .textContent = formattedResultString;
  }
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
   *     storageKeyEntries: {
   *       <storage_type>: [{
   *         bucketId: <bigint>,
   *         name: <string>,
   *         usage: <bigint>,
   *         useCount: <bigint>,
   *         lastAccessed: <Time>,
   *         lastModified: <Time>
   *          }]
   *        }
   *      }
   *    }
   */

  for (let i = 0; i < bucketTableEntries.length; i++) {
    const entry = bucketTableEntries[i];
    const bucketTableEntryObj: StorageTypeBucketTableEntry = {
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
        'storageKeyEntries': {},
      };
    }
    if (!(entry.type in bucketTableEntriesByStorageKey[entry.storageKey]
                                                      ['storageKeyEntries'])) {
      bucketTableEntriesByStorageKey[entry
                                         .storageKey]['storageKeyEntries'][entry
                                                                               .type] =
          [bucketTableEntryObj];
      bucketTableEntriesByStorageKey[entry.storageKey]['bucketCount'] += 1;
    } else {
      bucketTableEntriesByStorageKey[entry
                                         .storageKey]['storageKeyEntries'][entry
                                                                               .type]
          .push(bucketTableEntryObj);
      bucketTableEntriesByStorageKey[entry.storageKey]['bucketCount'] += 1;
    }
  }

  const storageKeys: string[] = Object.keys(bucketTableEntriesByStorageKey);

  /* Populate the rows of the Usage and Quota table by iterating over:
   * each storage key in bucketTableEntriesByStorageKey,
   * each storage key's storage type(s),
   * and each storage type's bucket(s). */

  // Iterate over each storageKey in bucketTableEntriesByStorageKey.
  for (let i = 0; i < storageKeys.length; i++) {
    const storageKey: string = storageKeys[i];
    const storageKeyRowSpan =
        bucketTableEntriesByStorageKey[storageKey]['bucketCount'];
    const bucketsByStorageType: StorageTypeEntries =
        bucketTableEntriesByStorageKey[storageKey]['storageKeyEntries'];
    const storageTypes: StorageType[] =
        Object.keys(bucketsByStorageType).map(typeStr => Number(typeStr));

    // Iterate over each storageType for a given storage key.
    for (let j = 0; j < storageTypes.length; j++) {
      const storageType: StorageType = storageTypes[j];
      const bucketsForStorageType: StorageTypeBucketTableEntry[] =
          bucketsByStorageType[storageType];
      const storageTypeRowSpan: number =
          bucketsByStorageType[storageType].length;

      // Iterate over each bucket for a given storageKey and storageType.
      for (let k = 0; k < bucketsForStorageType.length; k++) {
        const isFirstStorageKeyRow: boolean = (j === 0 && k === 0);
        const isFirstStorageType: boolean = (k === 0);

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

        usageAndQuotaRow.querySelector('.storage-key')!.textContent =
            storageKey;
        usageAndQuotaRow.querySelector('.storage-key')!.setAttribute(
            'rowspan', `${storageKeyRowSpan}`);
        usageAndQuotaRow.querySelector('.storage-type')!.textContent =
            StorageType[storageType];
        usageAndQuotaRow.querySelector('.storage-type')!.setAttribute(
            'rowspan', `${storageTypeRowSpan}`);
        usageAndQuotaRow.querySelector('.bucket')!.textContent =
            bucketsForStorageType[k].name;
        usageAndQuotaRow.querySelector('.usage')!.textContent =
            bucketsForStorageType[k].usage;
        usageAndQuotaRow.querySelector('.use-count')!.textContent =
            bucketsForStorageType[k].useCount;
        usageAndQuotaRow.querySelector('.last-accessed')!.textContent =
            bucketsForStorageType[k].lastAccessed;
        usageAndQuotaRow.querySelector('.last-modified')!.textContent =
            bucketsForStorageType[k].lastModified;

        /* If the current row is not the first of its kind for a given storage
         * key, remove the storage key cell when appending the row to the table
         * body. This creates a nested row to the right of the storage key cell.
         */
        if (!isFirstStorageKeyRow) {
          usageAndQuotaRow.querySelector('.storage-key')!.remove();
        }

        /* If the current storage type (temporary, syncable) is not
         * the first of its kind for a given storage key and storage type,
         * remove the Storage Type cells from the row before
         * appending the row to the table body.
         * This creates a nested row to the right of the Storage Type cell. */
        if (!isFirstStorageType) {
          usageAndQuotaRow.querySelector('.storage-type')!.remove();
        }
        tableBody.appendChild(usageAndQuotaRow);
      }
    }
  }
}

async function renderSimulateStoragePressureButton() {
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

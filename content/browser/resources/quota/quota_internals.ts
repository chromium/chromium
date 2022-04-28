// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {QuotaInternalsBrowserProxy} from './quota_internals_browser_proxy.js';

type BucketTableEntry = {
  'bucketId': bigint,
  'storageKey': string,
  'host': string,
  'type': string,
  'name': string,
  'useCount': bigint,
  'lastAccessed': Time,
  'lastModified': Time,
};

type RetrieveBucketsTableResult = {
  entries: BucketTableEntry[],
};

type HostAndStorageTypeBucketTableEntry = {
  'bucketId': string,
  'storageKey': string,
  'name': string,
  'useCount': string,
  'lastAccessed': string,
  'lastModified': string,
};

type StorageTypeHostUsageAndEntries = {
  'hostUsage': string,
  'entries': HostAndStorageTypeBucketTableEntry[],
};

type StorageTypeEntries = {
  // key = storageType
  [key: string]: StorageTypeHostUsageAndEntries,
};

type HostData = {
  'bucketCount': number,
  'hostEntries': StorageTypeEntries,
};

type BucketTableEntriesByHost = {
  // key = host
  [key: string]: HostData,
};

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

async function getHostUsageString(host: string, type: string): Promise<string> {
  const currentTotalUsageObj =
      await getProxy().getHostUsageForInternals(host, type);
  return currentTotalUsageObj.hostUsage.toString();
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
  const globalUsageStorageTypes: string[] =
      ['temporary', 'persistent', 'syncable'];

  for (const storageType of globalUsageStorageTypes) {
    const result = await getProxy().getGlobalUsage(storageType);
    const formattedResultString: string = `${Number(result.usage)} B (${
        result.unlimitedUsage} B for unlimited origins)`;
    document.body.querySelector(`.${storageType}-global-and-unlimited-usage`)!
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
  const bucketTableEntriesByHost: BucketTableEntriesByHost = {};

  /* Re-structure bucketTableEntries data to be accessible by a host key.
   * bucketTableEntriesByHost = {
   *   <host_string>: {
   *     bucketCount: <number>,
   *     hostEntries: {
   *       <storage_type_string>: {
   *         hostUsage: <string>,
   *         entries: [{
   *           bucketId: <bigint>,
   *           storageKey: <string>,
   *           host: <string>,
   *           type: <string>,
   *           name: <string>,
   *           useCount: <bigint>,
   *           lastAccessed: <Time>,
   *           lastModified: <Time>
   *          }]
   *        }
   *      }
   *    }
   */

  for (let i = 0; i < bucketTableEntries.length; i++) {
    const entry = bucketTableEntries[i];
    const bucketTableEntryObj: HostAndStorageTypeBucketTableEntry = {
      bucketId: entry.bucketId.toString(),
      storageKey: entry.storageKey,
      name: entry.name,
      useCount: entry.useCount.toString(),
      lastAccessed: convertMojoTimeToJS(entry.lastAccessed)
                        .toLocaleString('en-US', {timeZoneName: 'short'}),
      lastModified: convertMojoTimeToJS(entry.lastModified)
                        .toLocaleString('en-US', {timeZoneName: 'short'}),
    };

    if (!(entry.host in bucketTableEntriesByHost)) {
      bucketTableEntriesByHost[entry.host] = {
        'bucketCount': 0,
        'hostEntries': {}
      };
    }
    if (!(entry.type in bucketTableEntriesByHost[entry.host]['hostEntries'])) {
      bucketTableEntriesByHost[entry.host]['hostEntries'][entry.type] = {
        'hostUsage': await getHostUsageString(entry.host, entry.type),
        'entries': [bucketTableEntryObj]
      };
      bucketTableEntriesByHost[entry.host]['bucketCount'] += 1;
    } else {
      bucketTableEntriesByHost[entry.host]['hostEntries'][entry.type]['entries']
          .push(bucketTableEntryObj);
      bucketTableEntriesByHost[entry.host]['bucketCount'] += 1;
    }
  }

  const hostKeys: string[] = Object.keys(bucketTableEntriesByHost);

  /* Populate the rows of the Usage and Quota table by iterating over:
   * each host in bucketTableEntryByHost,
   * each host's storage type(s),
   * and each storage type's bucket(s). */

  // Iterate over each host key in bucketTableEntriesByHost.
  for (let i = 0; i < hostKeys.length; i++) {
    const host: string = hostKeys[i];
    const hostRowSpan = bucketTableEntriesByHost[host]['bucketCount'];
    const hostStorageTypes: StorageTypeEntries =
        bucketTableEntriesByHost[host]['hostEntries'];
    const storageTypes: string[] = Object.keys(hostStorageTypes);

    // Iterate over each storageType key for a given host.
    for (let j = 0; j < storageTypes.length; j++) {
      const storageType: string = storageTypes[j];
      const bucketsForStorageType: HostAndStorageTypeBucketTableEntry[] =
          hostStorageTypes[storageType]['entries'];
      const usageAndStorageTypeRowSpan: number =
          hostStorageTypes[storageType]['entries'].length;

      // Iterate over each bucket for a given host and storageType.
      for (let k = 0; k < bucketsForStorageType.length; k++) {
        const isFirstHostRow: boolean = (j === 0 && k === 0);
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

        usageAndQuotaRow.querySelector('.host')!.textContent = host;
        usageAndQuotaRow.querySelector('.host')!.setAttribute(
            'rowspan', `${hostRowSpan}`);
        usageAndQuotaRow.querySelector('.total-usage')!.textContent =
            hostStorageTypes[storageType]['hostUsage'];
        usageAndQuotaRow.querySelector('.total-usage')!.setAttribute(
            'rowspan', `${usageAndStorageTypeRowSpan}`);
        usageAndQuotaRow.querySelector('.storage-type')!.textContent =
            storageType;
        usageAndQuotaRow.querySelector('.storage-type')!.setAttribute(
            'rowspan', `${usageAndStorageTypeRowSpan}`);
        usageAndQuotaRow.querySelector('.storage-key')!.textContent =
            bucketsForStorageType[k].storageKey;
        usageAndQuotaRow.querySelector('.bucket')!.textContent =
            bucketsForStorageType[k].bucketId;
        usageAndQuotaRow.querySelector('.use-count')!.textContent =
            bucketsForStorageType[k].useCount;
        usageAndQuotaRow.querySelector('.last-accessed')!.textContent =
            bucketsForStorageType[k].lastAccessed;
        usageAndQuotaRow.querySelector('.last-modified')!.textContent =
            bucketsForStorageType[k].lastModified;

        /* If the current row is not the first of its kind for a given host,
         * remove the host cell when appending the row to the table body.
         * This creates a nested row to the right of the host cell. */
        if (!isFirstHostRow) {
          usageAndQuotaRow.querySelector('.host')!.remove();
        }

        /* If the current storage type (temporary, persistent, syncable) is not
         * the first of its kind for a given host and storage type,
         * remove the Total Usage and Storage Type cells from the row before
         * appending the row to the table body.
         * This creates a nested row to the right of the Storage Type cell. */
        if (!isFirstStorageType) {
          usageAndQuotaRow.querySelector('.total-usage')!.remove();
          usageAndQuotaRow.querySelector('.storage-type')!.remove();
        }
        tableBody.appendChild(usageAndQuotaRow);
      }
    }
  }
}

document.addEventListener('DOMContentLoaded', () => {
  renderDiskAvailabilityAndTempPoolSize();
  renderEvictionStats();
  renderGlobalUsage();
  renderUsageAndQuotaStats();
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
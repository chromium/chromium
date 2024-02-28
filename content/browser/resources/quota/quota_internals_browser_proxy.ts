// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import type {BucketTableEntry} from './quota_internals.mojom-webui.js';
import {QuotaInternalsHandler} from './quota_internals.mojom-webui.js';

export enum StorageType {
  TEMPORARY = 0,
  // PERSISTENT = 1, DEPRECATED
  SYNCABLE = 2,
}

interface GetDiskAvailabilityAndTempPoolSizeResult {
  totalSpace: bigint;
  availableSpace: bigint;
  tempPoolSize: bigint;
}

interface GetGlobalUsageResult {
  usage: bigint;
  unlimitedUsage: bigint;
}

interface SimulateStoragePressureAvailableResult {
  available: boolean;
}
export interface RetrieveBucketsTableResult {
  entries: BucketTableEntry[];
}

function urlPort(url: URL): number {
  if (url.port) {
    return Number.parseInt(url.port, 10);
  }
  if (url.protocol === 'https:') {
    return 443;
  } else if (url.protocol === 'http:') {
    return 80;
  } else {
    return 0;
  }
}

export class QuotaInternalsBrowserProxy {
  private handler = QuotaInternalsHandler.getRemote();

  getDiskAvailabilityAndTempPoolSize():
      Promise<GetDiskAvailabilityAndTempPoolSizeResult> {
    return this.handler.getDiskAvailabilityAndTempPoolSize();
  }

  getGlobalUsage(storageType: number): Promise<GetGlobalUsageResult> {
    return this.handler.getGlobalUsageForInternals(storageType);
  }

  getStatistics(): Promise<{evictionStatistics: {[key: string]: string}}> {
    return this.handler.getStatistics();
  }

  simulateStoragePressure() {
    const originToTest = (document.body.querySelector<HTMLInputElement>(
        '#origin-to-test'))!.value;
    const originUrl = new URL(originToTest);
    const newOrigin: Origin = {
      scheme: originUrl.protocol.replace(/:$/, ''),
      host: originUrl.host,
      port: urlPort(originUrl),
      nonceIfOpaque: null,
    };

    this.handler.simulateStoragePressure(newOrigin);
  }

  isSimulateStoragePressureAvailable():
      Promise<SimulateStoragePressureAvailableResult> {
    return this.handler.isSimulateStoragePressureAvailable();
  }

  retrieveBucketsTable(): Promise<RetrieveBucketsTableResult> {
    return this.handler.retrieveBucketsTable();
  }

  static getInstance(): QuotaInternalsBrowserProxy {
    return instance || (instance = new QuotaInternalsBrowserProxy());
  }
}

let instance: QuotaInternalsBrowserProxy|null = null;

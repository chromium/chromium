// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {QuotaInternalsHandler, QuotaInternalsHandlerRemote} from './quota_internals.mojom-webui.js';

type GetDiskAvailabilityResult = {
  totalSpace: bigint, availableSpace: bigint;
};

type GetStatisticsResult = {
  evictionStatistics: {
    'errors-on-getting-usage-and-quota': string,
    'evicted-buckets': string,
    'eviction-rounds': string,
    'skipped-eviction-rounds': string
  }
};

export class QuotaInternalsBrowserProxy {
  private handler = QuotaInternalsHandler.getRemote();

  getDiskAvailability(): Promise<GetDiskAvailabilityResult> {
    return this.handler.getDiskAvailability();
  }

  getStatistics(): Promise<GetStatisticsResult> {
    return this.handler.getStatistics();
  }

  static getInstance(): QuotaInternalsBrowserProxy {
    return instance || (instance = new QuotaInternalsBrowserProxy());
  }
}

let instance: QuotaInternalsBrowserProxy|null = null;
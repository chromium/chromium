// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';

import {QuotaInternalsHandler} from './quota_internals.mojom-webui.js';

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

  getDiskAvailability(): Promise<GetDiskAvailabilityResult> {
    return this.handler.getDiskAvailability();
  }

  getStatistics(): Promise<GetStatisticsResult> {
    return this.handler.getStatistics();
  }

  simulateStoragePressure() {
    const originToTest = (document.body.querySelector<HTMLInputElement>(
        '#origin-to-test'))!.value;
    const originUrl = new URL(originToTest);
    const newOrigin = new Origin;
    newOrigin.scheme = originUrl.protocol.replace(/:$/, '');
    newOrigin.host = originUrl.host;
    newOrigin.port = urlPort(originUrl);

    this.handler.simulateStoragePressure(newOrigin);
  }

  static getInstance(): QuotaInternalsBrowserProxy {
    return instance || (instance = new QuotaInternalsBrowserProxy());
  }
}

let instance: QuotaInternalsBrowserProxy|null = null;

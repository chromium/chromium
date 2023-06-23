// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_AVAILABILITY_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_AVAILABILITY_H_

#include <stdint.h>

namespace storage {
struct QuotaAvailability {
  int64_t available;
  int64_t total;

  QuotaAvailability(int64_t init_total, int64_t init_available) {
    available = init_available;
    total = init_total;
  }
};
}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_AVAILABILITY_H_

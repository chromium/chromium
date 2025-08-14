// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_device_info_helper.h"

#include "base/metrics/histogram_macros.h"

namespace storage {

QuotaDeviceInfoHelper::~QuotaDeviceInfoHelper() = default;

int64_t QuotaDeviceInfoHelper::AmountOfTotalDiskSpace(
    const base::FilePath& path) const {
  int64_t disk_space = base::SysInfo::AmountOfTotalDiskSpace(path);
  UMA_HISTOGRAM_BOOLEAN("Quota.TotalDiskSpaceIsZero", disk_space <= 0);
  return disk_space;
}

uint64_t QuotaDeviceInfoHelper::AmountOfPhysicalMemory() const {
  return base::SysInfo::AmountOfPhysicalMemory();
}

}  // namespace storage

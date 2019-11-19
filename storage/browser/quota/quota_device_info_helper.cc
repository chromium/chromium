// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_device_info_helper.h"

namespace storage {

QuotaDeviceInfoHelper::~QuotaDeviceInfoHelper() = default;

int64_t QuotaDeviceInfoHelper::AmountOfTotalDiskSpace(
    const base::FilePath& path) const {
  return base::SysInfo::AmountOfTotalDiskSpace(path);
}

int64_t QuotaDeviceInfoHelper::AmountOfPhysicalMemory() const {
  return base::SysInfo::AmountOfPhysicalMemory();
}

}  // namespace storage

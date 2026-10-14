// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_device_info_helper.h"

#include "base/byte_size.h"

namespace storage {

QuotaDeviceInfoHelper::~QuotaDeviceInfoHelper() = default;

int64_t QuotaDeviceInfoHelper::AmountOfTotalDiskSpace(
    const base::FilePath& path) const {
  return base::SysInfo::AmountOfTotalDiskSpace(path).value_or(-1);
}

uint64_t QuotaDeviceInfoHelper::AmountOfPhysicalMemory() const {
  return base::SysInfo::AmountOfTotalPhysicalMemory().InBytes();
}

}  // namespace storage

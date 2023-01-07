// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "base/system/sys_info.h"

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_DEVICE_INFO_HELPER_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_DEVICE_INFO_HELPER_H_

namespace storage {

// Interface used by the quota system to gather disk space information.
// Can be overridden in tests.
// Subclasses must be thread-safe.
// QuotaSettings instances own a singleton instance of QuotaDeviceInfoHelper.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaDeviceInfoHelper {
 public:
  QuotaDeviceInfoHelper() = default;

  QuotaDeviceInfoHelper(const QuotaDeviceInfoHelper&) = delete;
  QuotaDeviceInfoHelper& operator=(const QuotaDeviceInfoHelper&) = delete;

  virtual ~QuotaDeviceInfoHelper();

  virtual int64_t AmountOfTotalDiskSpace(const base::FilePath& path) const;

  virtual uint64_t AmountOfPhysicalMemory() const;
};  // class QuotaDeviceInfoHelper

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DEVICE_INFO_HELPER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_UTIL_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "storage/common/file_system/file_system_types.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {

class FileSystemContext;
class QuotaManagerProxy;
class QuotaReservation;

// An abstract interface that provides common quota-related utility functions
// for file_system_quota_client.
// All the methods of this class are synchronous and need to be called on
// the thread that the method name implies.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemQuotaUtil {
 public:
  virtual ~FileSystemQuotaUtil() = default;

  // Deletes the data on the origin and reports the amount of deleted data
  // to the quota manager via |proxy|.
  virtual base::File::Error DeleteStorageKeyDataOnFileTaskRunner(
      FileSystemContext* context,
      QuotaManagerProxy* proxy,
      const blink::StorageKey& storage_key,
      FileSystemType type) = 0;

  virtual void PerformStorageCleanupOnFileTaskRunner(FileSystemContext* context,
                                                     QuotaManagerProxy* proxy,
                                                     FileSystemType type) = 0;

  virtual std::vector<blink::StorageKey> GetStorageKeysForTypeOnFileTaskRunner(
      FileSystemType type) = 0;

  // Returns the amount of data used for the `storage_key` for usage tracking.
  virtual int64_t GetStorageKeyUsageOnFileTaskRunner(
      FileSystemContext* file_system_context,
      const blink::StorageKey& storage_key,
      FileSystemType type) = 0;

  // Creates new reservation object for the `storage_key` and the `type`.
  virtual scoped_refptr<QuotaReservation>
  CreateQuotaReservationOnFileTaskRunner(const blink::StorageKey& storage_key,
                                         FileSystemType type) = 0;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_QUOTA_UTIL_H_

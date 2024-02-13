// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_SHARED_STORAGE_CLEAR_SITE_DATA_TESTER_H_
#define CONTENT_BROWSER_BROWSING_DATA_SHARED_STORAGE_CLEAR_SITE_DATA_TESTER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "url/origin.h"

namespace content {

class SharedStorageClearSiteDataTester {
 public:
  explicit SharedStorageClearSiteDataTester(
      StoragePartition* storage_partition);

  void AddSharedStorageEntry(url::Origin origin,
                             std::u16string key,
                             std::u16string value);

  // Sets `total_to_add` entries that each concatenate the given `key` and
  // `value` with an index.
  void AddConsecutiveSharedStorageEntries(url::Origin origin,
                                          std::u16string key,
                                          std::u16string value,
                                          size_t total_to_add);

  std::vector<url::Origin> GetSharedStorageOrigins();

  int GetSharedStorageNumBytesForOrigin(url::Origin origin);

  int GetSharedStorageTotalBytes();

 private:
  raw_ptr<StoragePartitionImpl> storage_partition_impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_SHARED_STORAGE_CLEAR_SITE_DATA_TESTER_H_

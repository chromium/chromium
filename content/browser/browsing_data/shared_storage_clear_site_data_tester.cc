// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/shared_storage_clear_site_data_tester.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_database.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

// Converts the padded `total_size_bytes` stored for an origin back to number
// of entries in `values_mapping`.
[[nodiscard]] int PaddedBytesToNumEntries(int total_size_bytes) {
  return total_size_bytes /
         (storage::kSharedStorageEntryTotalBytesMultiplier *
          blink::features::kMaxSharedStorageStringLength.Get());
}

}  // namespace

SharedStorageClearSiteDataTester::SharedStorageClearSiteDataTester(
    StoragePartition* storage_partition)
    : storage_partition_impl_(
          static_cast<StoragePartitionImpl*>(storage_partition)) {}

void SharedStorageClearSiteDataTester::AddSharedStorageEntry(
    url::Origin origin,
    std::u16string key,
    std::u16string value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* shared_storage_manager =
      storage_partition_impl_->GetSharedStorageManager();
  DCHECK(shared_storage_manager);

  base::test::TestFuture<storage::SharedStorageDatabase::OperationResult>
      future;
  shared_storage_manager->Set(std::move(origin), std::move(key),
                              std::move(value), future.GetCallback());
  EXPECT_EQ(storage::SharedStorageDatabase::OperationResult::kSet,
            future.Get());
}

void SharedStorageClearSiteDataTester::AddConsecutiveSharedStorageEntries(
    url::Origin origin,
    std::u16string key,
    std::u16string value,
    size_t total_to_add) {
  for (size_t i = 0; i < total_to_add; ++i) {
    AddSharedStorageEntry(origin,
                          base::StrCat({key, base::NumberToString16(i)}),
                          base::StrCat({value, base::NumberToString16(i)}));
  }
}

std::vector<url::Origin>
SharedStorageClearSiteDataTester::GetSharedStorageOrigins() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* shared_storage_manager =
      storage_partition_impl_->GetSharedStorageManager();
  DCHECK(shared_storage_manager);

  base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
      future;
  shared_storage_manager->FetchOrigins(future.GetCallback());
  auto infos = future.Take();

  std::vector<url::Origin> origins;
  for (const auto& info : infos)
    origins.push_back(info->storage_key.origin());

  return origins;
}

int SharedStorageClearSiteDataTester::GetSharedStorageNumEntriesForOrigin(
    url::Origin origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* shared_storage_manager =
      storage_partition_impl_->GetSharedStorageManager();
  DCHECK(shared_storage_manager);

  base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
      future;
  shared_storage_manager->FetchOrigins(future.GetCallback());
  auto infos = future.Take();

  for (const auto& info : infos) {
    if (info->storage_key.origin() == origin)
      return PaddedBytesToNumEntries(info->total_size_bytes);
  }

  return 0;
}

int SharedStorageClearSiteDataTester::GetSharedStorageTotalEntries() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* shared_storage_manager =
      storage_partition_impl_->GetSharedStorageManager();
  DCHECK(shared_storage_manager);

  base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
      future;
  shared_storage_manager->FetchOrigins(future.GetCallback());
  auto infos = future.Take();

  int num_entries = 0;
  for (const auto& info : infos)
    num_entries += PaddedBytesToNumEntries(info->total_size_bytes);

  return num_entries;
}

}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/shared_dictionary_util.h"

#include <optional>

#include "base/byte_size.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

constexpr base::ByteSize kDefaultCacheMaxSizeForInMemory = base::MiBU(20);
constexpr base::ByteSize kMaxCacheMaxSizeForInMemory = base::MiBU(400);
constexpr base::ByteSize kDefaultCacheMaxSizeForOnDisk = base::MiBU(300);
constexpr base::ByteSize kMaxCacheMaxSizeForOnDisk = base::MiBU(400);

uint64_t CaliculateCacheMaxSizeForInMemory() {
  base::ByteSize cache_max_size = kDefaultCacheMaxSizeForInMemory;
  const base::ByteSize total_memory =
      base::SysInfo::AmountOfTotalPhysicalMemory();
  if (total_memory.is_zero()) {
    return cache_max_size.InBytes();
  }

  // We want to use up to 1% of the computer's memory, with a limit of 400 MB,
  // reached on system with more than 40 GB of RAM.
  cache_max_size = total_memory / 100;
  if (cache_max_size > kMaxCacheMaxSizeForInMemory) {
    cache_max_size = kMaxCacheMaxSizeForInMemory;
  }
  return cache_max_size.InBytes();
}

uint64_t CaliculateCacheMaxSizeForOnDisk(const base::FilePath& path) {
  base::ByteSize cache_max_size = kDefaultCacheMaxSizeForOnDisk;
  const std::optional<base::SysInfo::DiskSpaceInfo> disk_space =
      base::SysInfo::AmountOfDiskSpace(path);
  if (!disk_space) {
    return cache_max_size.InBytes();
  }

  // We want to use up to 1% of the available disk space, with a limit of 400
  // MB, reached on system with more than 40 GB of available disk space.
  cache_max_size = disk_space->available / 100;
  if (cache_max_size > kMaxCacheMaxSizeForOnDisk) {
    cache_max_size = kMaxCacheMaxSizeForOnDisk;
  }
  return cache_max_size.InBytes();
}

}  // namespace

void CalculateAndSetSharedDictionaryCacheMaxSize(
    base::WeakPtr<StoragePartition> storage_partition,
    const base::FilePath& path) {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          path.empty() ? base::BindOnce(&CaliculateCacheMaxSizeForInMemory)
                       : base::BindOnce(&CaliculateCacheMaxSizeForOnDisk, path),
          base::BindOnce(
              [](base::WeakPtr<StoragePartition> storage_partition,
                 uint64_t cache_max_size) {
                if (!storage_partition) {
                  return;
                }
                storage_partition->GetNetworkContext()
                    ->SetSharedDictionaryCacheMaxSize(cache_max_size);
              },
              storage_partition));
}

}  // namespace content

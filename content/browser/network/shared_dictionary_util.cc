// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/shared_dictionary_util.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

const uint64_t kDefaultCacheMaxSizeForInMemory = 10 * 1024 * 1024;  // 10 MiB;
const uint64_t kMaxCacheMaxSizeForInMemory = 200 * 1024 * 1024;     // 200 MiB;
const uint64_t kDefaultCacheMaxSizeForOnDisk = 150 * 1024 * 1024;   // 150 MiB;
const uint64_t kMaxCacheMaxSizeForOnDisk = 200 * 1024 * 1024;       // 200 MiB;

uint64_t CaliculateCacheMaxSizeForInMemory() {
  uint64_t cache_max_size = kDefaultCacheMaxSizeForInMemory;
  const uint64_t total_memory = base::SysInfo::AmountOfPhysicalMemory();
  if (total_memory == 0) {
    return cache_max_size;
  }

  // We want to use up to 1% of the computer's memory, with a limit of 200 MB,
  // reached on system with more than 20 GB of RAM.
  cache_max_size = total_memory / 100;
  if (cache_max_size > kMaxCacheMaxSizeForInMemory) {
    cache_max_size = kMaxCacheMaxSizeForInMemory;
  }
  return cache_max_size;
}

uint64_t CaliculateCacheMaxSizeForOnDisk(const base::FilePath& path) {
  uint64_t cache_max_size = kDefaultCacheMaxSizeForOnDisk;
  const int64_t available_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(path);
  if (available_disk_space <= 0) {
    return cache_max_size;
  }

  // We want to use up to 1% of the available disk space, with a limit of 200
  // MB, reached on system with more than 20 GB of available disk space.
  cache_max_size = available_disk_space / 100;
  if (cache_max_size > kMaxCacheMaxSizeForOnDisk) {
    cache_max_size = kMaxCacheMaxSizeForOnDisk;
  }
  return cache_max_size;
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

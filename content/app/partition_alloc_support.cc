// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/partition_alloc_support.h"

#include <string>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/extended_api.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/feature_list.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace content {
namespace internal {

namespace {

void EnablePCScanForMallocPartitionsIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_ALLOW_PCSCAN
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan)) {
    base::allocator::EnablePCScan();
  }
#endif
}

void EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_ALLOW_PCSCAN
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanBrowserOnly)) {
    base::allocator::EnablePCScan();
  }
#endif
}

// This function should be executed as early as possible once we can get the
// command line arguments and determine whether the process needs BRP support.
// Until that moment, all heap allocations end up in a slower temporary
// partition with no thread cache and cause heap fragmentation.
//
// Furthermore, since the function has to allocate a new partition, it must
// only run once.
void ConfigurePartitionRefCountSupportIfNeeded(bool enable_ref_count) {
// Note that ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL implies that
// USE_BACKUP_REF_PTR is true.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)
  base::allocator::ConfigurePartitionRefCountSupport(enable_ref_count);
#endif
}

void ReconfigurePartitionForKnownProcess(const std::string& process_type) {
  DCHECK_NE(process_type, switches::kZygoteProcess);

  // No specified process type means this is the Browser process.
  ConfigurePartitionRefCountSupportIfNeeded(process_type.empty());
}

}  // namespace

PartitionAllocSupport::PartitionAllocSupport() = default;

void PartitionAllocSupport::ReconfigureEarlyish(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
    CHECK(!called_earlyish_)
        << "ReconfigureEarlyish was already called for process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_earlyish_ = true;
    established_process_type_ = process_type;
  }

  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }

  // These initializations are only relevant for PartitionAlloc-Everywhere
  // builds.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  base::allocator::EnablePartitionAllocMemoryReclaimer();

#if defined(OS_ANDROID)
  // The thread cache consumes more memory, especially as long as periodic purge
  // above is disabled. Don't use one on low-memory devices.
  if (base::SysInfo::IsLowEndDevice()) {
    base::DisablePartitionAllocThreadCacheForProcess();
  }
#endif  // defined(OS_ANDROID)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::ReconfigureAfterZygoteFork(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
    CHECK(!called_after_zygote_fork_)
        << "ReconfigureAfterZygoteFork was already called for process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";
    DCHECK(called_earlyish_)
        << "Attempt to call ReconfigureAfterZygoteFork without calling "
           "ReconfigureEarlyish; current process: '"
        << process_type << "'";
    DCHECK_EQ(established_process_type_, switches::kZygoteProcess)
        << "Attempt to call ReconfigureAfterZygoteFork while "
           "ReconfigureEarlyish was called on non-zygote process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_after_zygote_fork_ = true;
    established_process_type_ = process_type;
  }

  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }
}

void PartitionAllocSupport::ReconfigureAfterFeatureListInit(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // Avoid initializing more than once.
    // TODO(bartekn): See if can be converted to (D)CHECK.
    if (called_after_feature_list_init_) {
      DCHECK_EQ(established_process_type_, process_type)
          << "ReconfigureAfterFeatureListInit was already called for process '"
          << established_process_type_ << "'; current process: '"
          << process_type << "'";
      return;
    }
    DCHECK(called_earlyish_)
        << "Attempt to call ReconfigureAfterFeatureListInit without calling "
           "ReconfigureEarlyish; current process: '"
        << process_type << "'";
    DCHECK_NE(established_process_type_, switches::kZygoteProcess)
        << "Attempt to call ReconfigureAfterFeatureListInit without calling "
           "ReconfigureAfterZygoteFork; current process: '"
        << process_type << "'";
    DCHECK_EQ(established_process_type_, process_type)
        << "ReconfigureAfterFeatureListInit wasn't called for an already "
           "established process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_after_feature_list_init_ = true;
  }

  DCHECK_NE(process_type, switches::kZygoteProcess);
  // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
  CHECK(base::FeatureList::GetInstance());

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::ReconfigurePartitionAllocLazyCommit();
#endif

  EnablePCScanForMallocPartitionsIfNeeded();
  // No specified process type means this is the Browser process.
  if (process_type.empty()) {
    EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded();
  }
}

void PartitionAllocSupport::ReconfigureAfterThreadPoolInit(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);

    // Init only once.
    if (called_after_thread_pool_init_)
      return;

    DCHECK_EQ(established_process_type_, process_type);
    // Enforce ordering.
    DCHECK(called_earlyish_);
    DCHECK(called_after_feature_list_init_);

    called_after_thread_pool_init_ = true;
  }

  // TODO(lizeb): This is only called from the browser process for now, extend
  // it to other processes as well.
  DCHECK(process_type.empty());
#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (process_type != switches::kZygoteProcess &&
      base::FeatureList::IsEnabled(
          base::features::kPartitionAllocThreadCachePeriodicPurge)) {
    base::internal::ThreadCacheRegistry::Instance().StartPeriodicPurge();
  }
#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace internal
}  // namespace content

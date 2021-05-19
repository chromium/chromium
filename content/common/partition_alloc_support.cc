// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/partition_alloc_support.h"

#include <string>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/extended_api.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace content {
namespace internal {

namespace {

void SetProcessNameForPCScan(const std::string& process_type) {
  const char* name = [&process_type] {
    if (process_type.empty()) {
      // Empty means browser process.
      return "Browser";
    }
    if (process_type == switches::kRendererProcess)
      return "Renderer";
    if (process_type == switches::kGpuProcess)
      return "Gpu";
    if (process_type == switches::kUtilityProcess)
      return "Utility";
    return static_cast<const char*>(nullptr);
  }();

  if (name) {
    base::internal::PCScan::SetProcessName(name);
  }
}

bool EnablePCScanForMallocPartitionsIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_ALLOW_PCSCAN
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan)) {
    base::allocator::EnablePCScan(/*dcscan*/ false);
    return true;
  }
#endif
  return false;
}

bool EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_ALLOW_PCSCAN
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanBrowserOnly)) {
    const bool dcscan_wanted =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan);
#if !defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK(!dcscan_wanted)
        << "DCScan is currently only supported on Linux based systems";
#endif
    base::allocator::EnablePCScan(dcscan_wanted);
    return true;
  }
#endif
  return false;
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

#if defined(OS_ANDROID)
  // The thread cache consumes more memory. Don't use one on low-memory devices
  // if thread cache purging is not enabled.
  if (base::SysInfo::IsLowEndDevice() &&
      !base::FeatureList::IsEnabled(
          base::features::kPartitionAllocThreadCachePeriodicPurge)) {
    base::DisablePartitionAllocThreadCacheForProcess();
  }
#endif  // defined(OS_ANDROID)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  bool scan_enabled = EnablePCScanForMallocPartitionsIfNeeded();
  // No specified process type means this is the Browser process.
  if (process_type.empty()) {
    scan_enabled = scan_enabled ||
                   EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded();
  }
  if (scan_enabled) {
    if (base::FeatureList::IsEnabled(
            base::features::kPartitionAllocPCScanStackScanning)) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
      base::internal::PCScan::EnableStackScanning();
      // Notify PCScan about the main thread.
      base::internal::PCScan::NotifyThreadCreated(
          base::internal::GetStackTop());
#endif
    }
    SetProcessNameForPCScan(process_type);
  }
}

void PartitionAllocSupport::ReconfigureAfterTaskRunnerInit(
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

#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // This should be called in specific processes, as the main thread is
  // initialized later.
  DCHECK(process_type != switches::kZygoteProcess);

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocThreadCachePeriodicPurge)) {
    auto& registry = base::internal::ThreadCacheRegistry::Instance();
    registry.StartPeriodicPurge();

#if defined(OS_ANDROID)
    // Lower thread cache limits to avoid stranding too much memory in the
    // caches.
    if (base::SysInfo::IsLowEndDevice()) {
      registry.SetThreadCacheMultiplier(
          base::internal::ThreadCache::kDefaultMultiplier / 2.);
    }
#endif  // defined(OS_ANDROID)
  }

  // Renderer processes are more performance-sensitive, increase thread cache
  // limits.
  if (process_type == switches::kRendererProcess &&
      base::FeatureList::IsEnabled(
          base::features::kPartitionAllocLargeThreadCacheSize)) {
#if defined(OS_ANDROID) && !defined(ARCH_CPU_64_BITS)
    // Don't use a higher threshold on Android 32 bits, as long as memory usage
    // is not carefully tuned. Only control the threshold here to avoid changing
    // the rest of the code below.
    // As of 2021, 64 bits Android devices are not memory constrained.
    largest_cached_size_ = base::internal::ThreadCache::kDefaultSizeThreshold;
#else
    largest_cached_size_ = base::internal::ThreadCache::kLargeSizeThreshold;
    base::internal::ThreadCache::SetLargestCachedSize(
        base::internal::ThreadCache::kLargeSizeThreshold);
#endif  // defined(OS_ANDROID) && !defined(ARCH_CPU_64_BITS)
  }

#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanMUAwareScheduler)) {
    // Assign PCScan a task-based scheduling backend.
    static base::NoDestructor<base::internal::MUAwareTaskBasedBackend>
        mu_aware_task_based_backend{
            base::internal::PCScan::scheduler(),
            base::BindRepeating([](base::TimeDelta delay) {
              base::internal::PCScan::PerformDelayedScan(delay);
            })};
    base::internal::PCScan::scheduler().SetNewSchedulingBackend(
        *mu_aware_task_based_backend.get());
  }
}

void PartitionAllocSupport::OnForegrounded() {
#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    base::AutoLock scoped_lock(lock_);
    if (established_process_type_ != switches::kRendererProcess)
      return;
  }

  base::internal::ThreadCache::SetLargestCachedSize(largest_cached_size_);
#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::OnBackgrounded() {
#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    base::AutoLock scoped_lock(lock_);
    if (established_process_type_ != switches::kRendererProcess)
      return;
  }

  // Performance matters less for background renderers, don't pay the memory
  // cost.
  //
  // TODO(lizeb): Consider forcing a one-off thread cache purge.
  base::internal::ThreadCache::SetLargestCachedSize(
      base::internal::ThreadCache::kDefaultSizeThreshold);
#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace internal
}  // namespace content

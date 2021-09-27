// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/partition_alloc_support.h"

#include <string>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/threading/thread_task_runner_handle.h"
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
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  using Config = base::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan)) {
    base::allocator::EnablePCScan({Config::WantedWriteProtectionMode::kEnabled,
                                   Config::SafepointMode::kEnabled});
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  return false;
}

bool EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  using Config = base::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanBrowserOnly)) {
    const Config::WantedWriteProtectionMode wp_mode =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan)
            ? Config::WantedWriteProtectionMode::kEnabled
            : Config::WantedWriteProtectionMode::kDisabled;
#if !defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK_EQ(Config::WantedWriteProtectionMode::kDisabled, wp_mode)
        << "DCScan is currently only supported on Linux based systems";
#endif
    base::allocator::EnablePCScan({wp_mode, Config::SafepointMode::kEnabled});
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  return false;
}

bool EnablePCScanForMallocPartitionsInRendererProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  using Config = base::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanRendererOnly)) {
    const Config::WantedWriteProtectionMode wp_mode =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan)
            ? Config::WantedWriteProtectionMode::kEnabled
            : Config::WantedWriteProtectionMode::kDisabled;
#if !defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK_EQ(Config::WantedWriteProtectionMode::kDisabled, wp_mode)
        << "DCScan is currently only supported on Linux based systems";
#endif
    base::allocator::EnablePCScan({wp_mode, Config::SafepointMode::kDisabled});
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  return false;
}

}  // namespace

void ReconfigurePartitionForKnownProcess(const std::string& process_type) {
  DCHECK_NE(process_type, switches::kZygoteProcess);
  // TODO(keishi): Move the code to enable BRP back here after Finch
  // experiments.
}

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

  bool enable_brp = false;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocBackupRefPtr)) {
    // No specified process type means this is the Browser process.
    enable_brp = process_type.empty();
    if (base::features::kBackupRefPtrEnabledProcessesParam.Get() ==
        base::features::BackupRefPtrEnabledProcesses::kBrowserAndRenderer) {
      enable_brp |= process_type == switches::kRendererProcess;
    }
  }
  base::allocator::ConfigurePartitionBackupRefPtrSupport(enable_brp);
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::ReconfigurePartitionAllocLazyCommit();
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // Don't enable PCScan if BRP is enabled.
  if (enable_brp)
    return;

  bool scan_enabled = EnablePCScanForMallocPartitionsIfNeeded();
  // No specified process type means this is the Browser process.
  if (process_type.empty()) {
    scan_enabled = scan_enabled ||
                   EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded();
  }
  if (process_type == switches::kRendererProcess) {
    scan_enabled = scan_enabled ||
                   EnablePCScanForMallocPartitionsInRendererProcessIfNeeded();
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
    if (base::FeatureList::IsEnabled(
            base::features::kPartitionAllocPCScanImmediateFreeing)) {
      base::internal::PCScan::EnableImmediateFreeing();
    }
    if (base::FeatureList::IsEnabled(
            base::features::kPartitionAllocPCScanEagerClearing)) {
      base::internal::PCScan::SetClearType(
          base::internal::PCScan::ClearType::kEager);
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

  auto& registry = base::internal::ThreadCacheRegistry::Instance();
  registry.StartPeriodicPurge();

#if defined(OS_ANDROID)
  // Lower thread cache limits to avoid stranding too much memory in the caches.
  if (base::SysInfo::IsLowEndDevice()) {
    registry.SetThreadCacheMultiplier(
        base::internal::ThreadCache::kDefaultMultiplier / 2.);
  }
#endif  // defined(OS_ANDROID)

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
    largest_cached_size_ =
        base::internal::ThreadCacheLimits::kDefaultSizeThreshold;
#else
    largest_cached_size_ =
        base::internal::ThreadCacheLimits::kLargeSizeThreshold;
    base::internal::ThreadCache::SetLargestCachedSize(
        base::internal::ThreadCacheLimits::kLargeSizeThreshold);
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

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::PartitionAllocMemoryReclaimer::Instance()->Start(
      base::ThreadTaskRunnerHandle::Get());
#endif
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
      base::internal::ThreadCacheLimits::kDefaultSizeThreshold);
#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace internal
}  // namespace content
